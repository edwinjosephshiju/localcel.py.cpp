#include "../common/installer_ui.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

namespace {
    std::string W2S_UI(const std::wstring& wstr) {
        std::string str;
        for (wchar_t c : wstr) {
            str.push_back(static_cast<char>(c));
        }
        return str;
    }
}

InstallerUI::InstallerUI() : nativeHandle(nullptr) {
}

InstallerUI::~InstallerUI() {
    Close();
}

bool InstallerUI::Create() {
    return true;
}

void InstallerUI::Show() {
    FILE* pipe = popen("zenity --progress --title=\"Localcel Bootstrapper\" --text=\"Initializing Setup...\" --percentage=0 --auto-close --no-cancel 2>/dev/null", "w");
    if (pipe) {
        nativeHandle = pipe;
    }

    std::wcout << L"======================================\n";
    std::wcout << L"         Localcel Bootstrapper        \n";
    std::wcout << L"======================================\n";
}

void InstallerUI::UpdateProgress(const std::wstring& statusText, int percentage) {
    if (nativeHandle) {
        FILE* pipe = static_cast<FILE*>(nativeHandle);
        std::string text = W2S_UI(statusText);
        fprintf(pipe, "%d\n", percentage);
        fprintf(pipe, "# %s\n", text.c_str());
        fflush(pipe);
    }
    std::wcout << L"[" << percentage << L"%] " << statusText << std::endl;
}

void InstallerUI::RunMessageLoop() {
}

void InstallerUI::Close() {
    if (nativeHandle) {
        FILE* pipe = static_cast<FILE*>(nativeHandle);
        fprintf(pipe, "100\n");
        fflush(pipe);
        pclose(pipe);
        nativeHandle = nullptr;
    }
}

void InstallerUI::ShowError(const std::wstring& errorMsg) {
    std::string error = W2S_UI(errorMsg);
    std::string cmd = "zenity --error --title=\"Localcel Error\" --text=\"" + error + "\" 2>/dev/null";
    std::system(cmd.c_str());
    std::wcerr << L"[ERROR] " << errorMsg << std::endl;
}
