#include "logger.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <iostream>

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const std::wstring& logDir) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::filesystem::create_directories(logDir);
    logFilePath = logDir + L"\\app.log";
    logFile.open(logFilePath, std::ios::app);
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::Log(const std::wstring& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[256];
    swprintf_s(timeBuf, L"[%04d-%02d-%02d %02d:%02d:%02d] INFO: ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    
    if (logFile.is_open()) {
        logFile << timeBuf << message << std::endl;
        logFile.flush();
    }
    wprintf(L"%s%s\n", timeBuf, message.c_str());
}

void Logger::LogError(const std::wstring& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[256];
    swprintf_s(timeBuf, L"[%04d-%02d-%02d %02d:%02d:%02d] ERROR: ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    
    if (logFile.is_open()) {
        logFile << timeBuf << message << std::endl;
        logFile.flush();
    }
    fwprintf(stderr, L"%s%s\n", timeBuf, message.c_str());
}
