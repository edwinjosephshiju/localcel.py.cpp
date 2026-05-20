#include "../common/dependency_manager.h"
#include "../common/process_util.h"
#include "../common/logger.h"

DependencyManager::DependencyManager(ProgressCallback cb) : onProgress(cb) {}

bool DependencyManager::InstallDependencies(bool updateMode) {
    if (!EnsurePythonRuntime()) return false;
    if (!EnsureGit()) return false;
    if (!EnsureCloudflared()) return false;
    if (!EnsurePythonPackages(updateMode)) return false;
    
    onProgress(L"All dependencies verified.", 100);
    return true;
}

bool DependencyManager::EnsureWinget() { return true; }
bool DependencyManager::EnsurePythonManager() { return true; }

bool DependencyManager::EnsurePythonRuntime() {
    onProgress(L"Checking Python Runtime...", 30);
    
    auto pyNormal = ProcessUtil::RunHidden(L"python3 --version", true);
    if (pyNormal.exitCode == 0) {
        Logger::GetInstance().Log(L"Python 3 detected.");
        pythonCommand = L"python3";
        return true;
    }

    onProgress(L"Installing Python 3 via apt...", 40);
    auto instRes = ProcessUtil::RunHidden(L"sudo apt-get update && sudo apt-get install -y python3 python3-pip python3-venv");
    if (instRes.exitCode != 0) {
        Logger::GetInstance().LogError(L"Failed to install Python 3. Please install manually.");
        return false;
    }
    pythonCommand = L"python3";
    return true;
}

bool DependencyManager::EnsureCloudflared() {
    onProgress(L"Checking cloudflared...", 60);
    if (!ProcessUtil::IsCommandAvailable(L"cloudflared")) {
        Logger::GetInstance().LogError(L"cloudflared not found. Please install manually on Linux for now.");
        // Not failing the build since it might not be strictly needed just to start GUI
    }
    return true;
}

bool DependencyManager::EnsureGit() {
    onProgress(L"Checking Git...", 75);
    if (!ProcessUtil::IsCommandAvailable(L"git")) {
        onProgress(L"Installing Git via apt...", 80);
        ProcessUtil::RunHidden(L"sudo apt-get install -y git");
    }
    return true;
}

bool DependencyManager::EnsureGitHubCLI() { return true; }

bool DependencyManager::EnsurePythonPackages(bool updateMode) {
    onProgress(L"Checking Python Packages...", 95);
    
    ProcessUtil::RunHidden(pythonCommand + L" -m venv venv", true);
    std::wstring pipCommand = L"./venv/bin/pip";
    std::wstring pyCommand = L"./venv/bin/python";
    pythonCommand = pyCommand; 
    
    if (!updateMode) {
        auto chk = ProcessUtil::RunHidden(pyCommand + L" -c \"import PyQt6, psutil, packaging\"", true);
        if (chk.exitCode == 0) {
            Logger::GetInstance().Log(L"Python packages already installed in venv.");
            return true;
        }
    }
    
    onProgress(updateMode ? L"Updating Python Packages..." : L"Installing Python Packages...", 98);
    std::wstring cmd = pipCommand + L" install " + (updateMode ? L"--upgrade " : L"") + L"PyQt6 psutil packaging";
    
    auto res = ProcessUtil::RunHidden(cmd, true);
    if (res.exitCode != 0) {
        Logger::GetInstance().LogError(L"Failed to install Python packages in venv.");
        return false;
    }
    return true;
}
