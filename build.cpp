#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>

int run_command(const std::string& cmd) {
    std::cout << "[BUILD] " << cmd << "\n";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[ERROR] Command failed with exit code " << ret << "\n";
    }
    return ret;
}

bool generate_resource_header(const std::string& inputFile, const std::string& arrayName, std::ofstream& out) {
    std::ifstream in(inputFile, std::ios::binary);
    if (!in) {
        std::cerr << "[ERROR] Could not open " << inputFile << " for embedding.\n";
        return false;
    }
    
    out << "const unsigned char " << arrayName << "[] = {\n";
    unsigned char c;
    int count = 0;
    while (in.read(reinterpret_cast<char*>(&c), 1)) {
        out << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)c << ", ";
        if (++count % 16 == 0) out << "\n";
    }
    out << "\n};\n";
    out << "const unsigned int " << arrayName << "_len = " << std::dec << count << ";\n\n";
    return true;
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
    
    if (!fs::exists("src/bootstrapper/common/stb_image.h")) {
        std::cout << "[BUILD] Downloading stb_image.h...\n";
        if (std::system("curl -s -o src/bootstrapper/common/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h") != 0) {
            std::system("wget -q -O src/bootstrapper/common/stb_image.h https://raw.githubusercontent.com/nothings/stb/master/stb_image.h");
        }
    }
    
    std::cout << "[BUILD] Generating embedded resources for Linux...\n";
    {
        std::ofstream resOut("src/bootstrapper/linux/generated_resources.h");
        if (!resOut) {
            std::cerr << "[ERROR] Failed to create generated_resources.h\n";
            return 1;
        }
        resOut << "#pragma once\n\n";
        
        bool ok = true;
        ok &= generate_resource_header("src/localcel.py", "localcel_py", resOut);
        ok &= generate_resource_header("assets/localcel_full.png", "localcel_full_png", resOut);
        ok &= generate_resource_header("assets/localcel_logo.ico", "localcel_logo_ico", resOut);
        
        if (!ok) {
            std::cerr << "[ERROR] Resource generation failed.\n";
            return 1;
        }
    }

    std::cout << "[BUILD] Compiling Linux build...\n";
    
    std::string clCmd = "g++ -std=c++20 -O2 -I src/bootstrapper/common ";
    clCmd += "src/bootstrapper/linux/main.cpp src/bootstrapper/common/logger.cpp ";
    clCmd += "src/bootstrapper/linux/installer_ui.cpp src/bootstrapper/linux/dependency_manager.cpp ";
    clCmd += "src/bootstrapper/linux/extractor.cpp src/bootstrapper/linux/hash_util.cpp ";
    clCmd += "src/bootstrapper/linux/process_util.cpp src/bootstrapper/linux/supervisor.cpp ";
    clCmd += "-o dist/Localcel $(pkg-config --cflags --libs gtk+-3.0)";
    
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
