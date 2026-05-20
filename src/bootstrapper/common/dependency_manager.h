#pragma once
#include <functional>
#include <string>

class DependencyManager {
public:
    using ProgressCallback = std::function<void(const std::wstring& status, int percentage)>;
    
    DependencyManager(ProgressCallback cb);
    
    bool InstallDependencies(bool updateMode = false);
    std::wstring GetPythonCommand() const { return pythonCommand; }

private:
    ProgressCallback onProgress;
    std::wstring pythonCommand = L"py -3.13";

    bool EnsureWinget();
    bool EnsurePythonManager();
    bool EnsurePythonRuntime();
    bool EnsureCloudflared();
    bool EnsureGit();
    bool EnsureGitHubCLI();
#if defined(__linux__)
    bool EnsureSystemDependencies();
#endif
    bool EnsurePythonPackages(bool updateMode);
};
