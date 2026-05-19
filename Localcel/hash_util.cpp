#include "hash_util.h"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((long)(Status)) >= 0)
#endif

#pragma comment(lib, "bcrypt.lib")

std::wstring BytesToHex(const std::vector<uint8_t>& bytes) {
    std::wstringstream wss;
    wss << std::hex << std::setfill(L'0');
    for (auto b : bytes) {
        wss << std::setw(2) << (int)b;
    }
    return wss.str();
}

std::wstring HashUtil::CalculateSHA256(const std::vector<uint8_t>& data) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD cbHashObject = 0, cbData = 0, cbHash = 0;
    std::vector<uint8_t> pbHashObject;
    std::vector<uint8_t> pbHash;

    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0))) return L"";

    if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    pbHashObject.resize(cbHashObject);

    if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    pbHash.resize(cbHash);

    if (!NT_SUCCESS(BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    if (!NT_SUCCESS(BCryptHashData(hHash, (PBYTE)data.data(), (ULONG)data.size(), 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    if (!NT_SUCCESS(BCryptFinishHash(hHash, pbHash.data(), cbHash, 0))) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return L"";
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return BytesToHex(pbHash);
}

std::wstring HashUtil::CalculateFileSHA256(const std::wstring& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return L"";

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read((char*)buffer.data(), size)) {
        return CalculateSHA256(buffer);
    }
    return L"";
}
