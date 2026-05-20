#include "../common/installer_ui.h"
#include "../common/logger.h"
#include "generated_resources.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "../common/stb_image.h"

namespace {
    std::string W2S_UI(const std::wstring& wstr) {
        std::string str;
        for (wchar_t c : wstr) {
            str.push_back(static_cast<char>(c));
        }
        return str;
    }

    struct X11WindowData {
        Display* display = nullptr;
        Window window = 0;
        GC gc = nullptr;
        int screen = 0;
        XImage* logoXImage = nullptr;
        int logoW = 0;
        int logoH = 0;
        std::atomic<int> progressPercent{0};
        std::atomic<bool> isRunning{false};
        std::string statusText{"Initializing..."};
        std::thread eventThread;
        XFontStruct* fontInfo = nullptr;
    };
    
    void X11EventLoop(X11WindowData* data, const unsigned char* pngData, unsigned int pngLen) {
        data->display = XOpenDisplay(NULL);
        if (!data->display) {
            std::cerr << "[X11] Failed to open X display! Falling back to CLI mode." << std::endl;
            data->isRunning = false;
            return;
        }
        
        data->screen = DefaultScreen(data->display);
        int depth = DefaultDepth(data->display, data->screen);
        Visual* visual = DefaultVisual(data->display, data->screen);
        
        int screenW = DisplayWidth(data->display, data->screen);
        int screenH = DisplayHeight(data->display, data->screen);
        int width = 500;
        int height = 200;
        int x = (screenW - width) / 2;
        int y = (screenH - height) / 2;
        
        XSetWindowAttributes attrs;
        attrs.background_pixel = 0x1E1E1E;
        attrs.override_redirect = True; // Flat borderless
        
        data->window = XCreateWindow(
            data->display, RootWindow(data->display, data->screen),
            x, y, width, height, 0,
            depth, InputOutput, visual,
            CWBackPixel | CWOverrideRedirect, &attrs
        );
        
        XStoreName(data->display, data->window, "Localcel Setup");
        XSelectInput(data->display, data->window, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
        XMapWindow(data->display, data->window);
        
        data->gc = XCreateGC(data->display, data->window, 0, NULL);
        
        // Font selection
        data->fontInfo = XLoadQueryFont(data->display, "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1");
        if (!data->fontInfo) {
            data->fontInfo = XLoadQueryFont(data->display, "fixed");
        }
        if (data->fontInfo) {
            XSetFont(data->display, data->gc, data->fontInfo->fid);
        }
        
        // Load embedded PNG logo
        int nChannels;
        unsigned char* decoded = stbi_load_from_memory(pngData, pngLen, &data->logoW, &data->logoH, &nChannels, 4);
        if (decoded) {
            unsigned char* bgra = (unsigned char*)malloc(data->logoW * data->logoH * 4);
            for (int i = 0; i < data->logoW * data->logoH; i++) {
                bgra[i*4 + 0] = decoded[i*4 + 2]; // B
                bgra[i*4 + 1] = decoded[i*4 + 1]; // G
                bgra[i*4 + 2] = decoded[i*4 + 0]; // R
                bgra[i*4 + 3] = decoded[i*4 + 3]; // A
            }
            stbi_image_free(decoded);
            
            data->logoXImage = XCreateImage(
                data->display, visual, depth, ZPixmap, 0,
                (char*)bgra, data->logoW, data->logoH, 32, 0
            );
        }
        
        bool isDragging = false;
        int dragStartX = 0;
        int dragStartY = 0;
        
        while (data->isRunning) {
            while (XPending(data->display) > 0) {
                XEvent ev;
                XNextEvent(data->display, &ev);
                if (ev.type == ButtonPress) {
                    if (ev.xbutton.button == 1) {
                        isDragging = true;
                        dragStartX = ev.xbutton.x;
                        dragStartY = ev.xbutton.y;
                    }
                } else if (ev.type == ButtonRelease) {
                    if (ev.xbutton.button == 1) {
                        isDragging = false;
                    }
                } else if (ev.type == MotionNotify) {
                    if (isDragging) {
                        int newWinX = ev.xmotion.x_root - dragStartX;
                        int newWinY = ev.xmotion.y_root - dragStartY;
                        XMoveWindow(data->display, data->window, newWinX, newWinY);
                    }
                }
            }
            
            // Render Background
            XSetForeground(data->display, data->gc, 0x1E1E1E);
            XFillRectangle(data->display, data->window, data->gc, 0, 0, width, height);
            
            // Draw window border (thin gray border)
            XSetForeground(data->display, data->gc, 0x3A3A3A);
            XDrawRectangle(data->display, data->window, data->gc, 0, 0, width - 1, height - 1);
            
            // Render Logo
            if (data->logoXImage) {
                int imgX = (width - data->logoW) / 2;
                XPutImage(data->display, data->window, data->gc, data->logoXImage, 0, 0, imgX, 20, data->logoW, data->logoH);
            }
            
            // Render Progress Bar Background (0x323232)
            XSetForeground(data->display, data->gc, 0x323232);
            XFillRectangle(data->display, data->window, data->gc, 20, 110, 460, 20);
            
            // Render Progress Bar Fill (0x0078D7)
            XSetForeground(data->display, data->gc, 0x0078D7);
            int percent = data->progressPercent.load();
            int fillW = (int)(460.0f * (percent / 100.0f));
            XFillRectangle(data->display, data->window, data->gc, 20, 110, fillW, 20);
            
            // Render Text (0xF0F0F0)
            XSetForeground(data->display, data->gc, 0xF0F0F0);
            std::string text = data->statusText + " (" + std::to_string(percent) + "%)";
            XDrawString(data->display, data->window, data->gc, 20, 95, text.c_str(), text.length());
            
            XFlush(data->display);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        if (data->logoXImage) {
            XDestroyImage(data->logoXImage);
        }
        if (data->fontInfo) {
            XFreeFont(data->display, data->fontInfo);
        }
        XFreeGC(data->display, data->gc);
        XDestroyWindow(data->display, data->window);
        XCloseDisplay(data->display);
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
    X11WindowData* data = new X11WindowData();
    data->isRunning = true;
    nativeHandle = data;
    
    data->eventThread = std::thread(X11EventLoop, data, localcel_full_png, localcel_full_png_len);
}

void InstallerUI::UpdateProgress(const std::wstring& statusText, int percentage) {
    if (nativeHandle) {
        X11WindowData* data = static_cast<X11WindowData*>(nativeHandle);
        data->statusText = W2S_UI(statusText);
        data->progressPercent.store(percentage);
    }
    std::wcout << L"[" << percentage << L"%] " << statusText << std::endl;
}

void InstallerUI::RunMessageLoop() {
}

void InstallerUI::Close() {
    if (nativeHandle) {
        X11WindowData* data = static_cast<X11WindowData*>(nativeHandle);
        data->isRunning = false;
        if (data->eventThread.joinable()) {
            data->eventThread.join();
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
