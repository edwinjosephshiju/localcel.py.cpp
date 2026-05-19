#include "process_util.h"
#include <windows.h>
#include "logger.h"

ProcessResult ProcessUtil::RunHidden(const std::wstring& commandLine, bool captureOutput) {
    ProcessResult result = { -1, "" };
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;

    if (captureOutput) {
        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
            LOG_ERR(L"CreatePipe failed");
            return result;
        }
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (captureOutput) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdError = hStdOutWrite;
        si.hStdOutput = hStdOutWrite;
    }

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmd = commandLine; // mutable copy for CreateProcessW

    LOG_INFO(L"Executing: " + cmd);

    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        if (captureOutput) {
            CloseHandle(hStdOutWrite);
            hStdOutWrite = NULL;

            DWORD bytesRead;
            char buffer[4096];
            while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result.output += buffer;
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            result.exitCode = exitCode;
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        LOG_ERR(L"CreateProcessW failed for: " + cmd);
    }

    if (hStdOutRead) CloseHandle(hStdOutRead);
    if (hStdOutWrite) CloseHandle(hStdOutWrite);

    return result;
}

bool ProcessUtil::IsCommandAvailable(const std::wstring& command) {
    std::wstring cmd = L"cmd.exe /c where " + command;
    ProcessResult res = RunHidden(cmd, false);
    return res.exitCode == 0;
}
