#include "../common/installer_ui.h"
#include "../common/logger.h"
#include "generated_resources.h"
#include <gtk/gtk.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>

namespace {
    std::string W2S_UI(const std::wstring& wstr) {
        std::string str;
        for (wchar_t c : wstr) {
            str.push_back(static_cast<char>(c));
        }
        return str;
    }

    struct GtkWindowData {
        GtkWidget* window = nullptr;
        GtkWidget* progress_bar = nullptr;
        GtkWidget* label = nullptr;
        GtkWidget* image = nullptr;
        std::thread gtkThread;
        std::atomic<bool> isRunning{false};
        std::atomic<int> progressPercent{0};
        std::string statusText{"Initializing..."};
        std::mutex mtx;
    };

    gboolean update_ui_callback(gpointer data) {
        GtkWindowData* wData = static_cast<GtkWindowData*>(data);
        std::lock_guard<std::mutex> lock(wData->mtx);
        
        std::string text = wData->statusText + " (" + std::to_string(wData->progressPercent.load()) + "%)";
        gtk_label_set_text(GTK_LABEL(wData->label), text.c_str());
        
        double frac = wData->progressPercent.load() / 100.0;
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(wData->progress_bar), frac);
        
        return wData->isRunning.load() ? TRUE : FALSE;
    }

    void GTKEventLoop(GtkWindowData* data, const unsigned char* pngData, unsigned int pngLen) {
        int argc = 0;
        char** argv = nullptr;
        if (!gtk_init_check(&argc, &argv)) {
            std::cerr << "[GTK] Failed to initialize GTK display! Falling back to CLI mode." << std::endl;
            data->isRunning = false;
            return;
        }

        data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(data->window), "Localcel Setup");
        gtk_window_set_default_size(GTK_WINDOW(data->window), 500, 200);
        gtk_window_set_position(GTK_WINDOW(data->window), GTK_WIN_POS_CENTER);
        gtk_window_set_decorated(GTK_WINDOW(data->window), FALSE); // Borderless!
        
        GtkCssProvider* provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(provider,
            "window { background-color: #1e1e1e; border: 1px solid #3a3a3a; }\n"
            "label { color: #f0f0f0; font-family: 'Segoe UI', sans-serif; font-size: 10pt; }\n"
            "progressbar trough { background-color: #323232; border: none; min-height: 20px; }\n"
            "progressbar progress { background-color: #0078d7; border: none; }\n",
            -1, NULL);
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        g_object_unref(provider);

        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
        gtk_container_add(GTK_CONTAINER(data->window), vbox);

        // Logo Image Loader
        GdkPixbufLoader* loader = gdk_pixbuf_loader_new_with_type("png", NULL);
        if (loader) {
            gdk_pixbuf_loader_write(loader, pngData, pngLen, NULL);
            gdk_pixbuf_loader_close(loader, NULL);
            GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
            if (pixbuf) {
                data->image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_box_pack_start(GTK_BOX(vbox), data->image, FALSE, FALSE, 0);
            }
        }

        GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(spacer, 1, 10);
        gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

        data->label = gtk_label_new("Initializing...");
        gtk_label_set_xalign(GTK_LABEL(data->label), 0.0);
        gtk_box_pack_start(GTK_BOX(vbox), data->label, FALSE, FALSE, 0);

        data->progress_bar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), data->progress_bar, FALSE, FALSE, 0);

        // Window drag functionality
        g_signal_connect(data->window, "button-press-event", G_CALLBACK(+[](GtkWidget* widget, GdkEventButton* event, gpointer data) -> gboolean {
            if (event->button == 1) {
                gtk_window_begin_move_drag(GTK_WINDOW(widget), event->button, event->x_root, event->y_root, event->time);
                return TRUE;
            }
            return FALSE;
        }), NULL);

        gtk_widget_show_all(data->window);

        g_timeout_add(16, update_ui_callback, data);

        gtk_main();
    }
}

InstallerUI::InstallerUI() : nativeHandle(nullptr) {
}

InstallerUI::~InstallerUI() {
    Close();
}

bool InstallerUI::Create() {
    return true;
}

void InstallerUI::Show() {
    GtkWindowData* data = new GtkWindowData();
    data->isRunning = true;
    nativeHandle = data;
    
    data->gtkThread = std::thread(GTKEventLoop, data, localcel_full_png, localcel_full_png_len);
}

void InstallerUI::UpdateProgress(const std::wstring& statusText, int percentage) {
    if (nativeHandle) {
        GtkWindowData* data = static_cast<GtkWindowData*>(nativeHandle);
        std::lock_guard<std::mutex> lock(data->mtx);
        data->statusText = W2S_UI(statusText);
        data->progressPercent.store(percentage);
    }
    std::wcout << L"[" << percentage << L"%] " << statusText << std::endl;
}

void InstallerUI::RunMessageLoop() {
}

void InstallerUI::Close() {
    if (nativeHandle) {
        GtkWindowData* data = static_cast<GtkWindowData*>(nativeHandle);
        data->isRunning = false;
        
        g_idle_add([](gpointer w) -> gboolean {
            gtk_main_quit();
            return FALSE;
        }, nullptr);

        if (data->gtkThread.joinable()) {
            data->gtkThread.join();
        }
        delete data;
        nativeHandle = nullptr;
    }
}

void InstallerUI::ShowError(const std::wstring& errorMsg) {
    std::string error = W2S_UI(errorMsg);
    std::string cmd = "zenity --error --title=\"Localcel Error\" --text=\"" + error + "\" 2>/dev/null";
    if (std::system(cmd.c_str())) {}
    std::wcerr << L"[ERROR] " << errorMsg << std::endl;
}
