#include "../common/hash_util.h"

std::wstring HashUtil::CalculateSHA256(const std::vector<uint8_t>& data) {
    // Basic placeholder for Linux: skip hash checking for now to avoid OpenSSL dependency
    return L"";
}

std::wstring HashUtil::CalculateFileSHA256(const std::wstring& filePath) {
    // Basic placeholder for Linux
    return L"";
}
