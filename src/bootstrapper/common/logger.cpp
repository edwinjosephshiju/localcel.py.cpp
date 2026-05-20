#include "logger.h"
#include <filesystem>
#include <iostream>
#include <chrono>
#include <ctime>

namespace {
    std::wstring GetCurrentTimeStr() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &now_time);
#else
        localtime_r(&now_time, &tm_buf);
#endif
        wchar_t timeBuf[256];
        std::wcsftime(timeBuf, sizeof(timeBuf)/sizeof(wchar_t), L"[%Y-%m-%d %H:%M:%S] ", &tm_buf);
        return std::wstring(timeBuf);
    }
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const std::wstring& logDir) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::filesystem::create_directories(logDir);
    logFilePath = logDir + L"/app.log";
    logFile.open(std::filesystem::path(logFilePath), std::ios::app);
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::Log(const std::wstring& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::wstring fullMsg = GetCurrentTimeStr() + L"INFO: " + message;
    
    if (logFile.is_open()) {
        logFile << fullMsg << std::endl;
        logFile.flush();
    }
    std::wcout << fullMsg << std::endl;
}

void Logger::LogError(const std::wstring& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::wstring fullMsg = GetCurrentTimeStr() + L"ERROR: " + message;
    
    if (logFile.is_open()) {
        logFile << fullMsg << std::endl;
        logFile.flush();
    }
    std::wcerr << fullMsg << std::endl;
}
