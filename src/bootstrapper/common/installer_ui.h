#pragma once
#include <windows.h>
#include <string>
#include <gdiplus.h>

class InstallerUI {
public:
    InstallerUI(HINSTANCE hInstance);
    ~InstallerUI();

    bool Create();
    void Show();
    void UpdateProgress(const std::wstring& statusText, int percentage);
    void RunMessageLoop();
    void Close();
    void ShowError(const std::wstring& errorMsg);

    HWND GetHWND() const { return hwnd; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInstance;
    HWND hwnd;
    HWND hwndProgressBar;
    HWND hwndStatusText;
    ULONG_PTR gdiplusToken;
    Gdiplus::Image* logoImage;
};
