#pragma once
#include <string>

class Extractor {
public:
    static bool ExtractResource(int resourceId, const wchar_t* resourceType, const std::wstring& outputPath, bool checkHash = true);
};
