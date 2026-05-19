#include "dependency_manager.h"
#include "process_util.h"
#include "logger.h"

DependencyManager::DependencyManager(ProgressCallback cb) : onProgress(cb) {}

bool DependencyManager::InstallDependencies(bool updateMode) {
    if (!EnsureWinget()) return false;
    if (!EnsurePythonRuntime()) return false;
    if (!EnsureCloudflared()) return false;
    if (!EnsureGit()) return false;
    if (!EnsureGitHubCLI()) return false;
    if (!EnsurePythonPackages(updateMode)) return false;
    
    onProgress(L"All dependencies verified.", 100);
    return true;
}

bool DependencyManager::EnsureWinget() {
    onProgress(L"Checking winget...", 5);
    if (!ProcessUtil::IsCommandAvailable(L"winget")) {
        LOG_ERR(L"Winget is not installed. Cannot proceed.");
        return false;
    }
    return true;
}

bool DependencyManager::EnsurePythonManager() {
    onProgress(L"Checking Python Manager...", 10);
    if (!ProcessUtil::IsCommandAvailable(L"py")) {
        onProgress(L"Installing Python Manager...", 15);
        auto res = ProcessUtil::RunHidden(L"winget install Python.PythonManager --silent --accept-package-agreements --accept-source-agreements");
        if (res.exitCode != 0) {
            LOG_ERR(L"Failed to install Python Manager");
            return false;
        }
    }
    return true;
}

bool DependencyManager::EnsurePythonRuntime() {
    onProgress(L"Checking Python Runtime...", 30);
    
    auto pyNormal = ProcessUtil::RunHidden(L"cmd.exe /c python --version", true);
    if (pyNormal.exitCode == 0) {
        LOG_INFO(L"Normal Python detected. Skipping Python installation.");
        pythonCommand = L"python";
        return true;
    }

    auto pyLauncher = ProcessUtil::RunHidden(L"cmd.exe /c py --version", true);
    if (pyLauncher.exitCode == 0) {
        LOG_INFO(L"Python Launcher detected. Skipping Python installation.");
        pythonCommand = L"py";
        return true;
    }

    if (!EnsurePythonManager()) return false;

    onProgress(L"Installing Python 3.13 Runtime...", 40);
    auto instRes = ProcessUtil::RunHidden(L"cmd.exe /c py install 3.13");
    if (instRes.exitCode != 0) {
        LOG_ERR(L"Failed to install Python 3.13 Runtime");
        return false;
    }
    pythonCommand = L"py -3.13";
    return true;
}

bool DependencyManager::EnsureCloudflared() {
    onProgress(L"Checking cloudflared...", 60);
    if (!ProcessUtil::IsCommandAvailable(L"cloudflared")) {
        onProgress(L"Installing cloudflared...", 65);
        auto res = ProcessUtil::RunHidden(L"winget install Cloudflare.cloudflared --silent --accept-package-agreements --accept-source-agreements");
        if (res.exitCode != 0) {
            LOG_ERR(L"Failed to install cloudflared");
            return false;
        }
    }
    return true;
}

bool DependencyManager::EnsureGit() {
    onProgress(L"Checking Git...", 75);
    if (!ProcessUtil::IsCommandAvailable(L"git")) {
        onProgress(L"Installing Git...", 80);
        auto res = ProcessUtil::RunHidden(L"winget install Git.Git --silent --accept-package-agreements --accept-source-agreements");
        if (res.exitCode != 0) {
            LOG_ERR(L"Failed to install Git");
            return false;
        }
    }
    return true;
}

bool DependencyManager::EnsureGitHubCLI() {
    onProgress(L"Checking GitHub CLI...", 90);
    if (!ProcessUtil::IsCommandAvailable(L"gh")) {
        onProgress(L"Installing GitHub CLI...", 95);
        auto res = ProcessUtil::RunHidden(L"winget install GitHub.cli --silent --accept-package-agreements --accept-source-agreements");
        if (res.exitCode != 0) {
            LOG_ERR(L"Failed to install GitHub CLI");
            return false;
        }
    }
    return true;
}

bool DependencyManager::EnsurePythonPackages(bool updateMode) {
    onProgress(L"Checking Python Packages...", 95);
    
    if (!updateMode) {
        auto chk = ProcessUtil::RunHidden(L"cmd.exe /c " + pythonCommand + L" -c \"import PyQt6, psutil, packaging\"", true);
        if (chk.exitCode == 0) {
            LOG_INFO(L"Python packages already installed.");
            return true;
        }
    }
    
    onProgress(updateMode ? L"Updating Python Packages..." : L"Installing Python Packages...", 98);
    std::wstring cmd = L"cmd.exe /c " + pythonCommand + L" -m pip install " + (updateMode ? L"--upgrade " : L"") + L"PyQt6 psutil packaging";
    
    auto res = ProcessUtil::RunHidden(cmd, true);
    if (res.exitCode != 0) {
        LOG_ERR(L"Failed to install Python packages.");
        return false;
    }
    return true;
}
