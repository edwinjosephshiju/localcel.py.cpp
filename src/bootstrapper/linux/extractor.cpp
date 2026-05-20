#include "../common/extractor.h"
#include "../common/logger.h"
#include "../common/hash_util.h"
#include <fstream>
#include <vector>
#include <filesystem>

bool Extractor::ExtractResource(const unsigned char* data, unsigned int size, const std::wstring& outputPath, bool checkHash) {
    std::vector<uint8_t> resData(data, data + size);
    std::filesystem::path p(outputPath);

    if (checkHash && std::filesystem::exists(p)) {
        std::wstring existingHash = HashUtil::CalculateFileSHA256(outputPath);
        std::wstring newHash = HashUtil::CalculateSHA256(resData);
        if (existingHash == newHash && !existingHash.empty()) {
            Logger::GetInstance().Log(L"File already up-to-date (hash match): " + outputPath);
            return true;
        }
    }

    std::ofstream file(p, std::ios::binary);
    if (!file.is_open()) {
        Logger::GetInstance().LogError(L"Failed to open output file for writing: " + outputPath);
        return false;
    }

    file.write((const char*)data, size);
    file.close();

    Logger::GetInstance().Log(L"Successfully extracted: " + outputPath);
    return true;
}
