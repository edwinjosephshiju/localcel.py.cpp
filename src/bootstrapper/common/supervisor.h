#pragma once
#include <string>
#include <windows.h>

class Supervisor {
public:
    static int RunAndWatch(const std::wstring& scriptPath, const std::wstring& pythonCmd);
};
