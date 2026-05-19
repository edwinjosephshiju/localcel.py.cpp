#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>

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

    namespace fs = std::filesystem;
    if (!fs::exists("dist")) {
        fs::create_directory("dist");
        std::cout << "[BUILD] Created 'dist' directory.\n";
    }

#if defined(_WIN32)
    std::cout << "[BUILD] OS detected: Windows\n";

    // 1. Compile Resources
    std::string rcCmd = "rc.exe /nologo /fo src\\bootstrapper\\windows\\resources.res src\\bootstrapper\\windows\\resources.rc";
    if (run_command(rcCmd) != 0) {
        std::cerr << "Failed to compile resources.rc\n";
        std::system("pause");
        return 1;
    }

    // 2. Compile Main Application
    std::string clCmd = "cl.exe /nologo /EHsc /std:c++20 /W3 /O2 ";
    clCmd += "/I src\\bootstrapper\\common ";
    clCmd += "/D \"NDEBUG\" /D \"_WINDOWS\" /D \"UNICODE\" /D \"_UNICODE\" ";
    
    std::vector<std::string> srcs = {
        "src\\bootstrapper\\windows\\main.cpp", "src\\bootstrapper\\windows\\installer_ui.cpp", "src\\bootstrapper\\windows\\dependency_manager.cpp", 
        "src\\bootstrapper\\windows\\extractor.cpp", "src\\bootstrapper\\windows\\hash_util.cpp", "src\\bootstrapper\\windows\\process_util.cpp", 
        "src\\bootstrapper\\windows\\supervisor.cpp", "src\\bootstrapper\\common\\logger.cpp"
    };

    for (const auto& src : srcs) {
        clCmd += src + " ";
    }

    clCmd += "src\\bootstrapper\\windows\\resources.res /link /SUBSYSTEM:WINDOWS /OUT:dist\\Localcel.exe ";
    
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
    std::system("del src\\bootstrapper\\windows\\*.obj src\\bootstrapper\\common\\*.obj src\\bootstrapper\\windows\\resources.res *.obj >nul 2>&1");

    std::cout << "\n==============================================\n";
    std::cout << " BUILD SUCCESSFUL: dist\\Localcel.exe generated.\n";
    std::cout << "==============================================\n";

    std::system("pause");
    return 0;

#elif defined(__linux__)
    std::cout << "[BUILD] OS detected: Linux\n";
    std::cout << "[BUILD] Compiling Linux placeholder...\n";
    
    std::string clCmd = "g++ -std=c++20 -O2 -I src/bootstrapper/common src/bootstrapper/linux/main.cpp src/bootstrapper/common/logger.cpp -o dist/Localcel";
    if (run_command(clCmd) != 0) {
        std::cerr << "Failed to compile C++ source files for Linux.\n";
        return 1;
    }
    
    std::cout << "\n==============================================\n";
    std::cout << " BUILD SUCCESSFUL: dist/Localcel generated.\n";
    std::cout << "==============================================\n";

    return 0;
#else
    std::cerr << "[ERROR] Unsupported OS detected.\n";
    return 1;
#endif
}
