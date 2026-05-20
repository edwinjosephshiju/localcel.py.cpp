#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <signal.h>
#include "../common/installer_ui.h"
#include "../common/logger.h"
#include "../common/dependency_manager.h"
#include "../common/extractor.h"
#include "../common/supervisor.h"
#include "generated_resources.h"

std::wstring GetLocalAppDataPath() {
    const char* homeDir = getenv("HOME");
    if (!homeDir) return L"";
    std::string path = std::string(homeDir) + "/.local/share";
    return std::wstring(path.begin(), path.end());
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    
    std::wstring localAppData = GetLocalAppDataPath();
    if (localAppData.empty()) {
        std::cerr << "Failed to get HOME path." << std::endl;
        return 1;
    }

    std::wstring installDir = localAppData + L"/Localcel";
    std::wstring logsDir = installDir + L"/logs";
    
    std::filesystem::create_directories(logsDir);
    Logger::GetInstance().Initialize(logsDir);
    Logger::GetInstance().Log(L"=== Application Started (Linux) ===");

    InstallerUI ui;
    ui.Show();

    bool updateMode = false;
    while (true) {
        DependencyManager depManager([&ui](const std::wstring& status, int percent) {
            ui.UpdateProgress(status, percent);
        });

        if (!depManager.InstallDependencies(updateMode)) {
            ui.ShowError(L"Failed to install dependencies. Check log for details.");
            break;
        }

        std::wstring runtimeDir = installDir + L"/runtime";
        std::filesystem::create_directories(runtimeDir);

        ui.UpdateProgress(L"Extracting resources...", 95);

        std::wstring pyPath = runtimeDir + L"/localcel.py";
        std::wstring pngPath = runtimeDir + L"/localcel_full.png";
        std::wstring icoPath = runtimeDir + L"/localcel_logo.ico";

        if (!Extractor::ExtractResource(localcel_py, localcel_py_len, pyPath, true)) {
            ui.ShowError(L"Failed to extract main python script.");
            break;
        }

        Extractor::ExtractResource(localcel_full_png, localcel_full_png_len, pngPath, true);
        Extractor::ExtractResource(localcel_logo_ico, localcel_logo_ico_len, icoPath, true);

        ui.UpdateProgress(L"Setup complete. Launching...", 100);

        int exitCode = Supervisor::RunAndWatch(pyPath, depManager.GetPythonCommand());
        if (exitCode == 42) {
            updateMode = true;
            ui.UpdateProgress(L"Preparing to update...", 0);
            continue;
        } else {
            break;
        }
    }

    Logger::GetInstance().Log(L"=== Application Exited ===");
    return 0;
}
