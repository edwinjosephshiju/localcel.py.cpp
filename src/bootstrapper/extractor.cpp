#include "extractor.h"
#include <windows.h>
#include <fstream>
#include <vector>
#include "hash_util.h"
#include "logger.h"
#include <filesystem>

bool Extractor::ExtractResource(int resourceId, const wchar_t* resourceType, const std::wstring& outputPath, bool checkHash) {
    HMODULE hModule = GetModuleHandle(NULL);
    HRSRC hResource = FindResourceW(hModule, MAKEINTRESOURCEW(resourceId), resourceType);
    if (!hResource) {
        LOG_ERR(L"Failed to find resource ID: " + std::to_wstring(resourceId));
        return false;
    }

    HGLOBAL hMemory = LoadResource(hModule, hResource);
    if (!hMemory) {
        LOG_ERR(L"Failed to load resource ID: " + std::to_wstring(resourceId));
        return false;
    }

    DWORD dwSize = SizeofResource(hModule, hResource);
    LPVOID lpAddress = LockResource(hMemory);
    if (!lpAddress || dwSize == 0) {
        LOG_ERR(L"Failed to lock resource ID: " + std::to_wstring(resourceId));
        return false;
    }

    std::vector<uint8_t> resData((uint8_t*)lpAddress, ((uint8_t*)lpAddress) + dwSize);

    if (checkHash && std::filesystem::exists(outputPath)) {
        std::wstring existingHash = HashUtil::CalculateFileSHA256(outputPath);
        std::wstring newHash = HashUtil::CalculateSHA256(resData);
        if (existingHash == newHash && !existingHash.empty()) {
            LOG_INFO(L"File already up-to-date (hash match): " + outputPath);
            return true;
        }
    }

    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERR(L"Failed to open output file for writing: " + outputPath);
        return false;
    }

    file.write((char*)resData.data(), resData.size());
    file.close();

    LOG_INFO(L"Successfully extracted: " + outputPath);
    return true;
}
