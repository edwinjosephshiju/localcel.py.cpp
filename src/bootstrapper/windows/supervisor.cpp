#include "supervisor.h"
#include "logger.h"

int Supervisor::RunAndWatch(const std::wstring& scriptPath, const std::wstring& pythonCmd) {
    LOG_INFO(L"Starting supervisor for: " + scriptPath);

    HANDLE hJob = CreateJobObjectW(NULL, NULL);
    if (!hJob) {
        LOG_ERR(L"Failed to create job object.");
        return 1;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
        LOG_ERR(L"Failed to set job object limits.");
        CloseHandle(hJob);
        return 1;
    }

    STARTUPINFOW si = { sizeof(si) };

    PROCESS_INFORMATION pi = { 0 };

    std::wstring cmd = pythonCmd + L" \"" + scriptPath + L"\"";

    LOG_INFO(L"Launching child: " + cmd);

    if (CreateProcessW(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
            LOG_ERR(L"Failed to assign process to job object.");
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hJob);
            return 1;
        }

        ResumeThread(pi.hThread);

        LOG_INFO(L"Child process launched successfully. Waiting for termination.");
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode = 1;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            LOG_INFO(L"Child process exited with code: " + std::to_wstring(exitCode));
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hJob);
        return exitCode;
    } else {
        LOG_ERR(L"CreateProcessW failed for python script.");
    }

    CloseHandle(hJob);
    LOG_INFO(L"Supervisor shutting down.");
    return 1;
}
