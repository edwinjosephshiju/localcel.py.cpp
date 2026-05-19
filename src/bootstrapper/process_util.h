#pragma once
#include <string>

struct ProcessResult {
    int exitCode;
    std::string output;
};

class ProcessUtil {
public:
    static ProcessResult RunHidden(const std::wstring& commandLine, bool captureOutput = false);
    static bool IsCommandAvailable(const std::wstring& command);
};
