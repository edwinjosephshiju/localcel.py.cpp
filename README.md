# <img src="assets/localcel_full.png" alt="Localcel Logo" width="250">

**A robust, production-grade native cross-platform C++ Bootstrapper and Supervisor for the Localcel Python GUI.**

> [!WARNING]
> **LINUX BUILD IS IN ALPHA STATE**: The Linux implementation of this bootstrapper is currently in an early **Alpha** stage. It is not considered fully stable and has only been tested on Ubuntu/Debian based environments. Please do not deploy it to production environments.

---

## Features & Platform Coverage

| Feature | Windows Support (Stable) | Linux Support (Alpha) |
| :--- | :---: | :---: |
| **Executable Footprint** | Extremely Lightweight (~700KB) | Lightweight (~750KB) |
| **Graphics Library** | GDI+ Native Win32 | GTK 3 (Supports native Wayland & X11) |
| **Dependencies Checked** | Python 3, `winget`, `cloudflared`, `gh` CLI | Python 3, `pip`, `venv`, `git`, `cloudflared` |
| **Admin Privilege Request** | Standard Windows UAC Prompt | Single GUI elevated Polkit (`pkexec`) dialog |
| **Process Supervision** | Windows Job Objects (Auto-kills children) | POSIX signal traps & child status monitoring |
| **Payload Delivery** | Windows Native `.rc` Resource payload | Pre-compiled header-embedded byte arrays |

---

## Architectural Breakdown

### 1. Cross-Platform UI Engine
* **Windows**: Implemented using GDI+ and native Win32 controls for a custom borderless dark-mode loader.
* **Linux**: Implemented using **GTK 3** which automatically targets the active desktop environment (**Wayland** or **X11**). It uses absolute pixel placements via `GtkFixed` to match the exact Windows UI footprint (500x200) and overrides theme configurations using custom inline CSS injectors. Drag-and-drop window movements are supported natively.
* **Headless Fallback**: On systems lacking a graphical server, the bootstrapper automatically detects the lack of display contexts and falls back to a clean terminal-based loading sequence.

### 2. Intelligent Dependency Management & Elevation
* Natively validates the existence of a Python environment, pip packages (`PyQt6`, `psutil`, `packaging`), and utility CLI clients (`git`, `cloudflared`).
* **Single Elevation Step**: To prevent requesting user credentials multiple times on Linux, the dependency manager batches missing system requirements and installs them all using a single root authentication call to `pkexec` (or `sudo` for terminal fallbacks).

### 3. Resource Extraction & Supervisor Watchdog
* Binary assets (such as PyQt Python code, configs, and icons) are embedded as byte arrays directly into the compiled executable structure.
* A watchdog supervisor monitors execution state: when the Python app requests a self-update (exit code `42`), the C++ loader captures the response, automatically hides the main app, recreates the installer UI, pulls the latest updates, and hot-swaps the run scripts.
* Safe child process destruction prevents orphan background processes on shutdown.

---

## Build Instructions

### **For Windows:**
Run the `build.cpp` natively from the "x64 Native Tools Command Prompt for VS 2022":
```cmd
if not exist dist mkdir dist
cl /EHsc /std:c++20 build.cpp /Fe:dist\build.exe
dist\build.exe
```
This compiles the MSVC build (`Localcel.exe`) directly into your `dist` folder.

### **For Linux (Alpha):**
1. Install GTK 3 development headers and X11 packages:
   ```bash
   sudo apt update && sudo apt install -y libgtk-3-dev libx11-dev
   ```
2. Build and run using standard GCC tools:
   ```bash
   g++ -std=c++20 build.cpp -o dist/build
   ./dist/build
   ./dist/Localcel
   ```

---

## GitHub Pages Deployment
The documentation for this repository is hosted on **GitHub Pages** directly from the `/docs` folder of the `master` branch. To serve changes:
1. Push modifications to `docs/index.html`.
2. Ensure your GitHub Repository setting under **Pages -> Build and deployment** is set to deploy from `/docs` folder of the `master` branch.
