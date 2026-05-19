#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

int run_command(const std::string& cmd) {
    std::cout << "[BUILD] " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[ERROR] Command failed with exit code " << ret << "\n";
    }
    return ret;
}

int main() {
    std::cout << "==============================================\n";
    std::cout << "      Localcel Native C++ Build System        \n";
    std::cout << "==============================================\n\n";

    // 1. Compile Resources
    std::string rcCmd = "rc.exe /nologo /fo src\\bootstrapper\\resources.res src\\bootstrapper\\resources.rc";
    if (run_command(rcCmd) != 0) {
        std::cerr << "Failed to compile resources.rc\n";
        std::system("pause");
        return 1;
    }

    // 2. Compile Main Application
    std::string clCmd = "cl.exe /nologo /EHsc /std:c++20 /W3 /O2 ";
    clCmd += "/D \"NDEBUG\" /D \"_WINDOWS\" /D \"UNICODE\" /D \"_UNICODE\" ";
    
    std::vector<std::string> srcs = {
        "src\\bootstrapper\\main.cpp", "src\\bootstrapper\\installer_ui.cpp", "src\\bootstrapper\\dependency_manager.cpp", 
        "src\\bootstrapper\\extractor.cpp", "src\\bootstrapper\\hash_util.cpp", "src\\bootstrapper\\process_util.cpp", 
        "src\\bootstrapper\\supervisor.cpp", "src\\bootstrapper\\logger.cpp"
    };

    for (const auto& src : srcs) {
        clCmd += src + " ";
    }

    clCmd += "src\\bootstrapper\\resources.res /link /SUBSYSTEM:WINDOWS /OUT:Localcel.exe ";
    
    std::vector<std::string> libs = {
        "user32.lib", "gdi32.lib", "comctl32.lib", 
        "bcrypt.lib", "shell32.lib", "advapi32.lib", "ole32.lib", "gdiplus.lib"
    };

    for (const auto& lib : libs) {
        clCmd += lib + " ";
    }

    if (run_command(clCmd) != 0) {
        std::cerr << "Failed to compile C++ source files.\n";
        std::system("pause");
        return 1;
    }

    // 3. Clean up intermediate files
    std::cout << "[BUILD] Cleaning up intermediate files (*.obj, resources.res)...\n";
    std::system("del src\\bootstrapper\\*.obj src\\bootstrapper\\resources.res *.obj >nul 2>&1");

    std::cout << "\n==============================================\n";
    std::cout << " BUILD SUCCESSFUL: Localcel.exe generated.\n";
    std::cout << "==============================================\n";

    std::system("pause");
    return 0;
}
