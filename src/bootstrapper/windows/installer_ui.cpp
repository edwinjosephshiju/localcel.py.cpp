#include "installer_ui.h"
#include <commctrl.h>
#include <vector>

#pragma comment(lib, "comctl32.lib")

#include "resource.h"
#include <Uxtheme.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "uxtheme.lib")

InstallerUI::InstallerUI(HINSTANCE hInst) : hInstance(hInst), hwnd(NULL), hwndProgressBar(NULL), hwndStatusText(NULL), logoImage(NULL) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(IDR_LOGO_PNG), RT_RCDATA);
    if (hRes) {
        DWORD imageSize = SizeofResource(hInstance, hRes);
        const void* pResourceData = LockResource(LoadResource(hInstance, hRes));
        if (pResourceData) {
            HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, imageSize);
            if (hBuffer) {
                void* pBuffer = GlobalLock(hBuffer);
                if (pBuffer) {
                    CopyMemory(pBuffer, pResourceData, imageSize);
                    IStream* pStream = NULL;
                    if (CreateStreamOnHGlobal(hBuffer, TRUE, &pStream) == S_OK) {
                        logoImage = new Gdiplus::Image(pStream);
                        pStream->Release();
                    }
                    GlobalUnlock(hBuffer);
                }
            }
        }
    }
}

InstallerUI::~InstallerUI() {
    if (logoImage) delete logoImage;
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

bool InstallerUI::Create() {
    const wchar_t CLASS_NAME[] = L"LocalcelInstallerClass";

    WNDCLASSEXW wc = { };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(105)); // IDI_APP_ICON
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(105));

    RegisterClassExW(&wc);

    int width = 500;
    int height = 200;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;

    hwnd = CreateWindowExW(
        0, // WS_EX_APPWINDOW
        CLASS_NAME,
        L"Localcel Setup",
        WS_POPUP | WS_BORDER, // Borderless modern window
        x, y, width, height,
        NULL, NULL, hInstance, this
    );

    if (!hwnd) {
        return false;
    }

    HFONT hFontStatus = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    hwndStatusText = CreateWindowExW(0, L"STATIC", L"Initializing...", WS_CHILD | WS_VISIBLE, 20, 80, 460, 20, hwnd, NULL, hInstance, NULL);
    SendMessage(hwndStatusText, WM_SETFONT, (WPARAM)hFontStatus, TRUE);

    hwndProgressBar = CreateWindowExW(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 20, 110, 460, 20, hwnd, NULL, hInstance, NULL);
    SetWindowTheme(hwndProgressBar, L" ", L" ");
    SendMessage(hwndProgressBar, PBM_SETBKCOLOR, 0, (LPARAM)RGB(50, 50, 50));
    SendMessage(hwndProgressBar, PBM_SETBARCOLOR, 0, (LPARAM)RGB(0, 120, 215));
    SendMessage(hwndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(hwndProgressBar, PBM_SETPOS, 0, 0);

    return true;
}

void InstallerUI::Show() {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void InstallerUI::UpdateProgress(const std::wstring& statusText, int percentage) {
    if (hwndStatusText) {
        std::wstring fullText = statusText + L" (" + std::to_wstring(percentage) + L"%)";
        SetWindowTextW(hwndStatusText, fullText.c_str());
    }
    if (hwndProgressBar) {
        SendMessage(hwndProgressBar, PBM_SETPOS, percentage, 0);
    }
}

void InstallerUI::Close() {
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = NULL;
    }
}

void InstallerUI::ShowError(const std::wstring& errorMsg) {
    MessageBoxW(hwnd, errorMsg.c_str(), L"Setup Error", MB_OK | MB_ICONERROR);
}

void InstallerUI::RunMessageLoop() {
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK InstallerUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    InstallerUI* pThis = NULL;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (InstallerUI*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (InstallerUI*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH hBgBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rc, hBgBrush);
            DeleteObject(hBgBrush);

            if (pThis && pThis->logoImage && pThis->logoImage->GetLastStatus() == Gdiplus::Ok) {
                Gdiplus::Graphics graphics(hdc);
                int imgW = pThis->logoImage->GetWidth();
                int imgH = pThis->logoImage->GetHeight();
                int maxW = 300;
                int maxH = 50;
                int finalW = imgW;
                int finalH = imgH;
                if (finalW > maxW) {
                    finalH = (int)((float)maxW / finalW * finalH);
                    finalW = maxW;
                }
                if (finalH > maxH) {
                    finalW = (int)((float)maxH / finalH * finalW);
                    finalH = maxH;
                }
                int x = (rc.right - finalW) / 2;
                int y = 20;
                graphics.DrawImage(pThis->logoImage, x, y, finalW, finalH);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(240, 240, 240));
            SetBkColor(hdcStatic, RGB(30, 30, 30));
            static HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
            return (INT_PTR)hBrush;
        }
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
            if (hit == HTCLIENT) return HTCAPTION; // Allow dragging by clicking anywhere
            return hit;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
