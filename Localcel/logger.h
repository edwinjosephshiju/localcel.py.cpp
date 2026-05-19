#pragma once

#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    static Logger& GetInstance();
    void Initialize(const std::wstring& logDir);
    void Log(const std::wstring& message);
    void LogError(const std::wstring& message);

private:
    Logger() = default;
    ~Logger();
    
    std::wstring logFilePath;
    std::wofstream logFile;
    std::mutex logMutex;
};

#define LOG_INFO(msg) Logger::GetInstance().Log(msg)
#define LOG_ERR(msg) Logger::GetInstance().LogError(msg)
