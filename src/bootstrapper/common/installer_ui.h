#pragma once
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <gdiplus.h>
#endif

class InstallerUI {
public:
#if defined(_WIN32)
    InstallerUI(HINSTANCE hInstance);
#else
    InstallerUI();
#endif
    ~InstallerUI();

    bool Create();
    void Show();
    void UpdateProgress(const std::wstring& statusText, int percentage);
    void RunMessageLoop();
    void Close();
    void ShowError(const std::wstring& errorMsg);

#if defined(_WIN32)
    HWND GetHWND() const { return hwnd; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInstance;
    HWND hwnd;
    HWND hwndProgressBar;
    HWND hwndStatusText;
    ULONG_PTR gdiplusToken;
    Gdiplus::Image* logoImage;
#endif
};
