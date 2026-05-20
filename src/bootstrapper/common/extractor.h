#pragma once
#include <string>
#include <cstdint>

class Extractor {
public:
#if defined(_WIN32)
    static bool ExtractResource(int resourceId, const wchar_t* resourceType, const std::wstring& outputPath, bool checkHash = true);
#else
    static bool ExtractResource(const unsigned char* data, unsigned int size, const std::wstring& outputPath, bool checkHash = true);
#endif
};
