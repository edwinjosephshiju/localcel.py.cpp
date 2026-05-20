#include "../common/supervisor.h"
#include "../common/logger.h"
#include <cstdlib>
#include <codecvt>
#include <locale>
#include <sys/wait.h>

namespace {
    std::string W2S(const std::wstring& wstr) {
        std::string str;
        for (wchar_t c : wstr) str.push_back(static_cast<char>(c));
        return str;
    }
}

int Supervisor::RunAndWatch(const std::wstring& scriptPath, const std::wstring& pythonCmd) {
    std::string py = W2S(pythonCmd);
    std::string sp = W2S(scriptPath);
    
    std::string cmd = py + " " + sp;
    Logger::GetInstance().Log(L"Supervisor launching: " + std::wstring(cmd.begin(), cmd.end()));
    
    int status = std::system(cmd.c_str());
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}
