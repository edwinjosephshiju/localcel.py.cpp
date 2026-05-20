#pragma once
#include <string>
#include <vector>
#include <cstdint>

class HashUtil {
public:
    static std::wstring CalculateSHA256(const std::vector<uint8_t>& data);
    static std::wstring CalculateFileSHA256(const std::wstring& filePath);
};
