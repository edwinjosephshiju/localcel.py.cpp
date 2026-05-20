#include "../common/installer_ui.h"
#include <iostream>

InstallerUI::InstallerUI() {}
InstallerUI::~InstallerUI() {}

bool InstallerUI::Create() { return true; }

void InstallerUI::Show() {
    std::wcout << L"======================================\n";
    std::wcout << L"         Localcel Bootstrapper        \n";
    std::wcout << L"======================================\n";
}

void InstallerUI::UpdateProgress(const std::wstring& statusText, int percentage) {
    std::wcout << L"[" << percentage << L"%] " << statusText << std::endl;
}

void InstallerUI::RunMessageLoop() {
    // Terminal UI does not need a message loop for Linux currently.
}

void InstallerUI::Close() {}

void InstallerUI::ShowError(const std::wstring& errorMsg) {
    std::wcerr << L"[ERROR] " << errorMsg << std::endl;
}
