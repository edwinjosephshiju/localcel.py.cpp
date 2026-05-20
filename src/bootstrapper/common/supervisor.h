#pragma once
#include <string>

class Supervisor {
public:
    static int RunAndWatch(const std::wstring& scriptPath, const std::wstring& pythonCmd);
};
