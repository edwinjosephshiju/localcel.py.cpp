#include <windows.h>
#include <string>
#include <thread>
#include <filesystem>
#include <shlobj.h>
#include "installer_ui.h"
#include "logger.h"
#include "dependency_manager.h"
#include "extractor.h"
#include "supervisor.h"
#include "resource.h"

std::wstring GetLocalAppDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::wstring(path);
    }
    return L"";
}

void SetupThread(InstallerUI* ui, const std::wstring& installDir) {
    bool updateMode = false;
    while (true) {
        try {
            DependencyManager depManager([ui](const std::wstring& status, int percent) {
                PostMessage(ui->GetHWND(), WM_USER + 1, percent, (LPARAM)new std::wstring(status));
            });

            if (!depManager.InstallDependencies(updateMode)) {
                PostMessage(ui->GetHWND(), WM_USER + 2, 0, (LPARAM)new std::wstring(L"Failed to install dependencies. Check log for details."));
                return;
            }

            std::wstring runtimeDir = installDir + L"\\runtime";
            std::filesystem::create_directories(runtimeDir);

            PostMessage(ui->GetHWND(), WM_USER + 1, 95, (LPARAM)new std::wstring(L"Extracting resources..."));

            std::wstring pyPath = runtimeDir + L"\\localcel.py";
            std::wstring manifestPath = runtimeDir + L"\\manifest.json";
            std::wstring pngPath = runtimeDir + L"\\localcel_full.png";
            std::wstring icoPath = runtimeDir + L"\\localcel_logo.ico";

            if (!Extractor::ExtractResource(IDR_PY_MAIN, RT_RCDATA, pyPath, true)) {
                PostMessage(ui->GetHWND(), WM_USER + 2, 0, (LPARAM)new std::wstring(L"Failed to extract main python script."));
                return;
            }

            Extractor::ExtractResource(IDR_MANIFEST_JSON, RT_RCDATA, manifestPath, false);
            Extractor::ExtractResource(IDR_LOGO_PNG, RT_RCDATA, pngPath, true);
            Extractor::ExtractResource(IDR_LOGO_ICO, RT_RCDATA, icoPath, true);

            PostMessage(ui->GetHWND(), WM_USER + 1, 100, (LPARAM)new std::wstring(L"Setup complete. Launching..."));
            Sleep(1000); // Give user a moment to see 100%

            PostMessage(ui->GetHWND(), WM_USER + 3, 0, 0); // Signal success and hide UI

            int exitCode = Supervisor::RunAndWatch(pyPath, depManager.GetPythonCommand());
            if (exitCode == 42) {
                updateMode = true;
                PostMessage(ui->GetHWND(), WM_USER + 4, 0, 0); // show UI again
                continue;
            } else {
                PostMessage(ui->GetHWND(), WM_USER + 5, 0, 0); // close entirely
                break;
            }

        } catch (const std::exception&) {
            LOG_ERR(L"Exception in setup thread.");
            PostMessage(ui->GetHWND(), WM_USER + 2, 0, (LPARAM)new std::wstring(L"An unexpected error occurred."));
            break;
        }
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
    }

    std::wstring localAppData = GetLocalAppDataPath();
    if (localAppData.empty()) {
        MessageBoxW(NULL, L"Failed to get LOCALAPPDATA path.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::wstring installDir = localAppData + L"\\Localcel";
    std::wstring logsDir = installDir + L"\\logs";
    
    std::filesystem::create_directories(logsDir);
    Logger::GetInstance().Initialize(logsDir);
    LOG_INFO(L"=== Application Started ===");

    InstallerUI ui(hInstance);
    if (!ui.Create()) {
        LOG_ERR(L"Failed to create UI.");
        return 1;
    }
    ui.Show();

    std::thread setupThread(SetupThread, &ui, installDir);

    MSG msg;
    bool keepRunning = true;
    while (keepRunning && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_USER + 1) {
            std::wstring* status = (std::wstring*)msg.lParam;
            ui.UpdateProgress(*status, (int)msg.wParam);
            delete status;
        } else if (msg.message == WM_USER + 2) {
            std::wstring* errorMsg = (std::wstring*)msg.lParam;
            ui.ShowError(*errorMsg);
            delete errorMsg;
            keepRunning = false;
        } else if (msg.message == WM_USER + 3) {
            ShowWindow(ui.GetHWND(), SW_HIDE);
        } else if (msg.message == WM_USER + 4) {
            ShowWindow(ui.GetHWND(), SW_SHOW);
            ui.UpdateProgress(L"Preparing to update...", 0);
        } else if (msg.message == WM_USER + 5) {
            ui.Close();
            keepRunning = false;
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (setupThread.joinable()) {
        setupThread.join();
    }

    LOG_INFO(L"=== Application Exited ===");
    return 0;
}
