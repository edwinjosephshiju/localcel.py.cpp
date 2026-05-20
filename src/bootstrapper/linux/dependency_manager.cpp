#include "../common/dependency_manager.h"
#include "../common/process_util.h"
#include "../common/logger.h"
#include <cstdlib>

DependencyManager::DependencyManager(ProgressCallback cb) : onProgress(cb) {}

bool DependencyManager::InstallDependencies(bool updateMode) {
    if (!EnsureSystemDependencies()) return false;
    if (!EnsurePythonRuntime()) return false;
    if (!EnsureGit()) return false;
    if (!EnsureCloudflared()) return false;
    if (!EnsurePythonPackages(updateMode)) return false;
    
    onProgress(L"All dependencies verified.", 100);
    return true;
}

bool DependencyManager::EnsureWinget() { return true; }
bool DependencyManager::EnsurePythonManager() { return true; }

bool DependencyManager::EnsureSystemDependencies() {
    bool needPython = !ProcessUtil::IsCommandAvailable(L"python3");
    bool needGit = !ProcessUtil::IsCommandAvailable(L"git");
    
    bool needVenv = false;
    if (!needPython) {
        auto checkVenv = ProcessUtil::RunHidden(L"python3 -m venv --help", true);
        if (checkVenv.exitCode != 0) {
            needVenv = true;
        }
    } else {
        needVenv = true;
    }

    std::wstring packages = L"";
    if (needPython) packages += L" python3 python3-pip";
    if (needVenv) packages += L" python3-venv";
    if (needGit) packages += L" git";

    if (!packages.empty()) {
        onProgress(L"Requesting administrator privileges to install dependencies...", 10);
        
        std::wstring aptCmd = L"apt-get update && apt-get install -y" + packages;
        
        std::wstring runner = L"sudo";
        if (ProcessUtil::IsCommandAvailable(L"pkexec")) {
            runner = L"pkexec";
        }
        
        std::wstring pkexecCmd = runner + L" sh -c \"" + aptCmd + L"\"";
        
        Logger::GetInstance().Log(L"Running elevation: " + pkexecCmd);
        auto res = ProcessUtil::RunHidden(pkexecCmd, true);
        if (res.exitCode != 0) {
            Logger::GetInstance().LogError(L"Elevation failed or user canceled.");
            return false;
        }
    }
    return true;
}

bool DependencyManager::EnsurePythonRuntime() {
    onProgress(L"Checking Python Runtime...", 30);
    
    auto pyNormal = ProcessUtil::RunHidden(L"python3 --version", true);
    if (pyNormal.exitCode == 0) {
        Logger::GetInstance().Log(L"Python 3 verified.");
        pythonCommand = L"python3";
        return true;
    }
    
    Logger::GetInstance().LogError(L"Python 3 runtime missing.");
    return false;
}

bool DependencyManager::EnsureCloudflared() {
    onProgress(L"Checking cloudflared...", 60);
    if (!ProcessUtil::IsCommandAvailable(L"cloudflared")) {
        Logger::GetInstance().LogError(L"cloudflared not found. Please install manually on Linux for now.");
    }
    return true;
}

bool DependencyManager::EnsureGit() {
    onProgress(L"Checking Git...", 75);
    if (ProcessUtil::IsCommandAvailable(L"git")) {
        Logger::GetInstance().Log(L"Git verified.");
        return true;
    }
    
    Logger::GetInstance().LogError(L"Git missing.");
    return false;
}

bool DependencyManager::EnsureGitHubCLI() { return true; }

bool DependencyManager::EnsurePythonPackages(bool updateMode) {
    onProgress(L"Checking Python Packages...", 95);
    
    std::wstring venvPath = L"venv";
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        std::string h(homeDir);
        venvPath = std::wstring(h.begin(), h.end()) + L"/.local/share/Localcel/venv";
    }
    
    auto venvRes = ProcessUtil::RunHidden(pythonCommand + L" -m venv " + venvPath, true);
    if (venvRes.exitCode != 0) {
        Logger::GetInstance().LogError(L"Failed to create virtual environment.");
        return false;
    }

    std::wstring pipCommand = venvPath + L"/bin/pip";
    std::wstring pyCommand = venvPath + L"/bin/python";
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
