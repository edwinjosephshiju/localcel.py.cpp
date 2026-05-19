# <img src="assets/localcel_full.png" alt="Localcel Logo" width="250">

**A robust, production-grade native Win32 C++ Bootstrapper and Supervisor for the Localcel Python GUI.**

## Architecture

- **Ultra-lightweight Native C++ Executable**: No .NET, No Qt overhead. Pure Win32 API.
- **Embedded Python Payload**: The PyQt6 application is entirely embedded inside the `.exe` as an `RCDATA` resource.
- **Intelligent Dependency Manager**: Native C++ detection for Python, `gh`, `git`, and `cloudflared`. Bypasses Python installation if an existing installation is found.
- **Native Updates**: Performs pip upgrades and extracts newest `.py` files automatically from within the C++ environment.
- **Process Supervisor**: Ensures no orphan processes. 
- **Dynamic Theming**: Custom dark-mode installer UI leveraging GDI+.

## Build Instructions

**For Windows:**
Run the `build.cpp` natively from the "x64 Native Tools Command Prompt for VS 2022":
```cmd
mkdir dist
cl /EHsc build.cpp /Fe:dist\build.exe
dist\build.exe
```
This will compile the `Localcel.exe` using standard Microsoft MSVC tools (`cl.exe`, `rc.exe`) into the `dist` directory.

**For Linux (Placeholder):**
```bash
mkdir -p dist
g++ -std=c++20 build.cpp -o dist/build
./dist/build
```
