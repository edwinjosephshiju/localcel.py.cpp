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

        // Create window with dialog/splash screen settings to ensure it doesn't get window decorations
        data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(data->window), "Localcel Setup");
        
        // Force exact window size to match Windows layout (500x200)
        gtk_window_set_default_size(GTK_WINDOW(data->window), 500, 200);
        gtk_widget_set_size_request(data->window, 500, 200);
        gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
        
        // Remove window decorations completely
        gtk_window_set_decorated(GTK_WINDOW(data->window), FALSE);
        
        // Hint the window manager that this is a splash/popup to center it and bypass general WM styling
        gtk_window_set_type_hint(GTK_WINDOW(data->window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
        gtk_window_set_position(GTK_WINDOW(data->window), GTK_WIN_POS_CENTER_ALWAYS);
        
        // Give custom names to apply styling rules
        gtk_widget_set_name(data->window, "installer-window");

        GtkCssProvider* provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(provider,
            "#installer-window {\n"
            "  background-color: #1e1e1e !important;\n"
            "  border: 1px solid #3a3a3a !important;\n"
            "}\n"
            "#installer-label {\n"
            "  color: #f0f0f0 !important;\n"
            "  font-family: 'Segoe UI', sans-serif !important;\n"
            "  font-size: 10pt !important;\n"
            "  background: none !important;\n"
            "  text-shadow: none !important;\n"
            "}\n"
            "progressbar trough {\n"
            "  background-color: #323232 !important;\n"
            "  border: none !important;\n"
            "  min-height: 20px !important;\n"
            "  border-radius: 0px !important;\n"
            "}\n"
            "progressbar progress {\n"
            "  background-color: #0078d7 !important;\n"
            "  border: none !important;\n"
            "  border-radius: 0px !important;\n"
            "}\n",
            -1, NULL);
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        g_object_unref(provider);

        // Fixed layout for exact alignment matching
        GtkWidget* fixed = gtk_fixed_new();
        gtk_container_add(GTK_CONTAINER(data->window), fixed);

        // Logo Image Loader
        GdkPixbufLoader* loader = gdk_pixbuf_loader_new_with_type("png", NULL);
        if (loader) {
            gdk_pixbuf_loader_write(loader, pngData, pngLen, NULL);
            gdk_pixbuf_loader_close(loader, NULL);
            GdkPixbuf* pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
            if (pixbuf) {
                int imgW = gdk_pixbuf_get_width(pixbuf);
                int imgX = (500 - imgW) / 2;
                data->image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_fixed_put(GTK_FIXED(fixed), data->image, imgX, 20);
            }
        }

        // Label (Status text at X=20, Y=80, W=460, H=20)
        data->label = gtk_label_new("Initializing...");
        gtk_widget_set_name(data->label, "installer-label");
        gtk_widget_set_size_request(data->label, 460, 20);
        gtk_label_set_xalign(GTK_LABEL(data->label), 0.0);
        gtk_fixed_put(GTK_FIXED(fixed), data->label, 20, 80);

        // Progress Bar (ProgressBar at X=20, Y=110, W=460, H=20)
        data->progress_bar = gtk_progress_bar_new();
        gtk_widget_set_size_request(data->progress_bar, 460, 20);
        gtk_fixed_put(GTK_FIXED(fixed), data->progress_bar, 20, 110);

        // Drag to move window
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
            GtkWindowData* d = static_cast<GtkWindowData*>(w);
            if (d->window) {
                gtk_widget_destroy(d->window);
                d->window = nullptr;
            }
            gtk_main_quit();
            return FALSE;
        }, data);

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
