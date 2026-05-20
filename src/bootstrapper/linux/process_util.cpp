#include "../common/process_util.h"
#include <cstdlib>
#include <array>
#include <memory>
#include <codecvt>
#include <locale>
#include <sys/wait.h>
#include <unistd.h>

namespace {
    std::string WStringToString(const std::wstring& wstr) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wstr);
    }
}

ProcessResult ProcessUtil::RunHidden(const std::wstring& commandLine, bool captureOutput) {
    std::string cmd = WStringToString(commandLine);
    ProcessResult result = { -1, "" };
    
    if (captureOutput) {
        cmd += " 2>&1";
        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            return result;
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result.output += buffer.data();
        }
        int status = pclose(pipe.release());
        result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    } else {
        cmd += " >/dev/null 2>&1";
        int status = std::system(cmd.c_str());
        result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
    return result;
}

bool ProcessUtil::IsCommandAvailable(const std::wstring& command) {
    std::wstring checkCmd = L"command -v " + command;
    auto res = RunHidden(checkCmd, false);
    return res.exitCode == 0;
}
