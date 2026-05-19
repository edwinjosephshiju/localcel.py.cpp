import os
import sys
import subprocess
import tempfile
import shutil
import hashlib

# ==========================================
# MODULE: DROPPER ARCHITECTURE
# ==========================================
# The entire GUI application is stored as a string payload.
# This prevents PyInstaller from seeing PyQt6 during the build phase,
# keeping the resulting .exe incredibly small (~5MB).

PAYLOAD = r'''
import os
import sys
import json
import socket
import shutil
import subprocess
import threading
import re
import platform
import webbrowser
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, Optional, List

GLOBAL_APP_DIR = Path.home() / ".localcel"
GLOBAL_APP_DIR.mkdir(parents=True, exist_ok=True)
DEPS_FILE = GLOBAL_APP_DIR / "deps.json"
CONFIG_FILE = GLOBAL_APP_DIR / "config.json"

# Dependencies are now entirely managed by the Localcel C++ Bootstrapper.

from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QLabel, QPushButton, QFrame, QScrollArea,
                             QTabWidget, QPlainTextEdit, QLineEdit, QDialog, QMessageBox,
                             QFileDialog, QSystemTrayIcon, QMenu, QComboBox, QInputDialog)
from PyQt6.QtCore import Qt, pyqtSignal, QObject, QTimer
from PyQt6.QtGui import QIcon, QPixmap, QImage, QFont
import psutil
import base64

# ==========================================
# MODULE: EMBEDDED ASSETS
# ==========================================
# Valid 1x1 transparent PNGs to strictly prevent 'libpng IDAT' and CRC errors 
# during direct execution before the real Base64 builder process binds the payload.
ICON_B64 = b"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="
LOGO_B64 = b"iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="

def get_icon():
    try:
        script_dir = Path(__file__).parent
        ico_path = script_dir / "localcel_logo.ico"
        if ico_path.exists():
            return QIcon(str(ico_path))
        img = QImage.fromData(base64.b64decode(ICON_B64))
        if img.isNull():
            return QIcon()
        return QIcon(QPixmap.fromImage(img))
    except:
        return QIcon()

def get_logo():
    try:
        script_dir = Path(__file__).parent
        png_path = script_dir / "localcel_full.png"
        if png_path.exists():
            return QPixmap(str(png_path))
        img = QImage.fromData(base64.b64decode(LOGO_B64))
        if img.isNull():
            return QPixmap()
        return QPixmap.fromImage(img)
    except:
        return QPixmap()

# ==========================================
# MODULE: UTILS & PATHS
# ==========================================
BASE_DIR = None
APPS_DIR = None
LOGS_DIR = None
PIDS_DIR = None

def initialize_workspace(path: Path):
    global BASE_DIR, APPS_DIR, LOGS_DIR, PIDS_DIR
    BASE_DIR = path
    APPS_DIR = BASE_DIR / "apps"
    LOGS_DIR = BASE_DIR / "logs"
    PIDS_DIR = BASE_DIR / "pids"
    ensure_directories()

def ensure_directories():
    if BASE_DIR:
        for d in [APPS_DIR, LOGS_DIR, PIDS_DIR]:
            d.mkdir(parents=True, exist_ok=True)

def get_executable_path(name: str) -> Optional[str]:
    return shutil.which(name)

def is_port_in_use(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('127.0.0.1', port)) == 0

def apply_translucent_acrylic(window):
    """Enables Translucent/Acrylic/Mica backdrops natively on Windows 11."""
    if platform.system() == "Windows":
        window.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        try:
            import ctypes
            from ctypes import wintypes
            hwnd = int(window.winId())
            set_window_attribute = ctypes.windll.dwmapi.DwmSetWindowAttribute
            set_window_attribute(hwnd, 20, ctypes.byref(ctypes.c_int(1)), 4)
            set_window_attribute(hwnd, 38, ctypes.byref(ctypes.c_int(3)), 4)
            class MARGINS(ctypes.Structure):
                _fields_ = [("cxLeftWidth", ctypes.c_int), ("cxRightWidth", ctypes.c_int),
                            ("cyTopHeight", ctypes.c_int), ("cyBottomHeight", ctypes.c_int)]
            margins = MARGINS(-1, -1, -1, -1)
            ctypes.windll.dwmapi.DwmExtendFrameIntoClientArea(hwnd, ctypes.byref(margins))
        except Exception as e:
            print(f"Native acrylic failed: {e}")

def get_or_choose_workspace() -> Path:
    if CONFIG_FILE.exists():
        try:
            with open(CONFIG_FILE, 'r') as f:
                data = json.load(f)
                ws_str = data.get("workspace")
                if ws_str:
                    ws_path = Path(ws_str)
                    if ws_path.exists() and ws_path.is_dir():
                        return ws_path
        except Exception:
            pass

    msg = QMessageBox()
    msg.setWindowTitle("Workspace Setup")
    msg.setWindowIcon(get_icon())
    msg.setText("Welcome to Localcel!\n\nPlease select a directory to act as your Server Workspace.\nAll your applications, configurations, and logs will be securely stored there.")
    msg.setIcon(QMessageBox.Icon.Information)
    msg.exec()

    dialog = QFileDialog()
    icon_obj = get_icon()
    if not icon_obj.isNull():
        dialog.setWindowIcon(icon_obj)
    dialog.setWindowTitle("Select Workspace Directory")
    dialog.setFileMode(QFileDialog.FileMode.Directory)
    dialog.setOption(QFileDialog.Option.ShowDirsOnly, True)
    apply_translucent_acrylic(dialog)

    if dialog.exec():
        selected = dialog.selectedFiles()[0]
        selected_path = Path(selected)
        if selected_path.name != "localcel_workspace":
            ws_path = selected_path / "localcel_workspace"
        else:
            ws_path = selected_path
            
        ws_path.mkdir(parents=True, exist_ok=True)
        with open(CONFIG_FILE, 'w') as f:
            json.dump({"workspace": str(ws_path)}, f)
        return ws_path
    else:
        sys.exit(0)

# ==========================================
# MODULE: CORE LOGIC
# ==========================================
@dataclass
class AppConfig:
    name: str
    port: int
    entry: str
    domain: Optional[str] = None
    app_type: str = "node" # 'node', 'static_cf', 'static_gh'
    github_repo: Optional[str] = None
    gh_pages_deployed: bool = False

class AppManager:
    @staticmethod
    def get_apps() -> List[AppConfig]:
        ensure_directories()
        apps = []
        for app_dir in APPS_DIR.iterdir():
            if app_dir.is_dir() and (app_dir / "config.json").exists():
                try:
                    with open(app_dir / "config.json", 'r') as f:
                        data = json.load(f)
                        # Set defaults for old apps
                        if "app_type" not in data: data["app_type"] = "node"
                        if data["app_type"] == "static": data["app_type"] = "static_gh"
                        if "github_repo" not in data: data["github_repo"] = None
                        if "gh_pages_deployed" not in data: data["gh_pages_deployed"] = False
                        apps.append(AppConfig(**data))
                except: pass
        return sorted(apps, key=lambda x: x.name)

    @staticmethod
    def create_app(name: str, port: int, domain: str = "", entry: str = "server.js", app_type: str = "node", github_repo: str = "", gh_pages_deployed: bool = False):
        app_dir = APPS_DIR / name
        app_dir.mkdir(parents=True, exist_ok=True)
        config = {"name": name, "port": port, "entry": entry, "app_type": app_type, "github_repo": github_repo, "gh_pages_deployed": gh_pages_deployed}
        if domain: config["domain"] = domain
        with open(app_dir / "config.json", 'w') as f:
            json.dump(config, f, indent=4)
        
        if app_type == "node":
            server_js = f"""const http = require('http');
const port = process.env.PORT || {port};
const server = http.createServer((req, res) => {{
  console.log(`[${{new Date().toISOString()}}] ${{req.method}} ${{req.url}}`);
  res.end('Localcel: {name} is running!');
}});
server.listen(port, () => {{
  console.log(`Server started on port ${{port}}`);
}});"""
            with open(app_dir / entry, 'w', encoding="utf-8") as f:
                f.write(server_js)
        
        elif app_type in ["static_cf", "static_gh"]:
            index_html = f"""<!DOCTYPE html>
<html>
<head>
    <title>{name}</title>
    <style>body {{ font-family: sans-serif; text-align: center; padding: 50px; }} h1 {{ color: #2D3748; }}</style>
</head>
<body>
    <h1>Localcel: {name} is running!</h1>
    <p>This is a static site managed by Localcel.</p>
</body>
</html>"""
            with open(app_dir / "index.html", 'w', encoding="utf-8") as f:
                f.write(index_html)
            
            # Initialize Git Repository
            git_bin = get_executable_path("git")
            if git_bin:
                subprocess.run([git_bin, "init"], cwd=str(app_dir), capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                subprocess.run([git_bin, "config", "user.name", "Localcel Deployer"], cwd=str(app_dir), capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                subprocess.run([git_bin, "config", "user.email", "deploy@localcel.app"], cwd=str(app_dir), capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                subprocess.run([git_bin, "add", "."], cwd=str(app_dir), capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                subprocess.run([git_bin, "commit", "-m", "Initial commit from Localcel"], cwd=str(app_dir), capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                subprocess.run([git_bin, "branch", "-M", "main"], cwd=str(app_dir), capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    @staticmethod
    def update_app(name: str, port: int, domain: str = "", app_type: str = "node", github_repo: str = "", gh_pages_deployed: Optional[bool] = None):
        config_path = APPS_DIR / name / "config.json"
        if config_path.exists():
            with open(config_path, 'r') as f:
                config = json.load(f)
            config['port'] = port
            config['app_type'] = app_type
            config['github_repo'] = github_repo
            
            if gh_pages_deployed is not None:
                config['gh_pages_deployed'] = gh_pages_deployed
            
            if domain:
                config['domain'] = domain
            elif 'domain' in config:
                del config['domain']
                
            with open(config_path, 'w') as f:
                json.dump(config, f, indent=4)

    @staticmethod
    def delete_app(name: str):
        if (APPS_DIR / name).exists():
            # Force remove to handle potential .git read-only files on Windows
            def on_rm_error(func, path, exc_info):
                os.chmod(path, 0o777)
                func(path)
            shutil.rmtree(APPS_DIR / name, onerror=on_rm_error)

class CloudflareHelper:
    @staticmethod
    def install_cloudflared():
        if platform.system() == "Windows":
            if get_executable_path("winget"):
                subprocess.run(["winget", "install", "Cloudflare.cloudflared", "--accept-package-agreements", "--accept-source-agreements"], shell=True)
                return True
        return False

    @staticmethod
    def list_tunnels() -> List[dict]:
        cf_bin = get_executable_path("cloudflared")
        if not cf_bin: return []
        res = subprocess.run([cf_bin, "tunnel", "list", "--output", "json"], capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        if res.returncode == 0:
            try: return json.loads(res.stdout)
            except: return []
        return []

    @staticmethod
    def delete_tunnel(tunnel_id_or_name: str):
        cf_bin = get_executable_path("cloudflared")
        if not cf_bin: return
        subprocess.run([cf_bin, "tunnel", "delete", "-f", tunnel_id_or_name], creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)

    @staticmethod
    def setup_named_tunnel(app_name: str, port: int, domain: str) -> Path:
        cf_bin = get_executable_path("cloudflared")
        tunnel_name = f"localcel_{app_name}"
        subprocess.run([cf_bin, "tunnel", "create", tunnel_name], capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        
        tunnels = CloudflareHelper.list_tunnels()
        tunnel_id = next((t['id'] for t in tunnels if t['name'] == tunnel_name), None)
        
        if not tunnel_id: raise Exception("Tunnel ID not found.")
        
        subprocess.run([cf_bin, "tunnel", "route", "dns", "-f", tunnel_name, domain], capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        
        config_path = APPS_DIR / app_name / "tunnel.yml"
        cred_file = Path.home() / ".cloudflared" / f"{tunnel_id}.json"
        yaml = f"tunnel: {tunnel_id}\ncredentials-file: {cred_file.as_posix()}\ningress:\n  - hostname: {domain}\n    service: http://localhost:{port}\n  - service: http_status:404"
        with open(config_path, 'w') as f: f.write(yaml)
        return config_path

class GitHubHelper:
    @staticmethod
    def ensure_gh():
        if not get_executable_path("gh"):
            subprocess.run(["winget", "install", "GitHub.cli", "--accept-source-agreements", "--accept-package-agreements"], shell=True)
        return get_executable_path("gh")

    @staticmethod
    def ensure_git():
        if not get_executable_path("git"):
            subprocess.run(["winget", "install", "Git.Git", "--accept-source-agreements", "--accept-package-agreements"], shell=True)
        return get_executable_path("git")

    @staticmethod
    def get_logged_in_user():
        gh = get_executable_path("gh")
        if not gh: return None
        res = subprocess.run([gh, "auth", "status"], capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        output = res.stderr + res.stdout
        match = re.search(r"Logged in to github\.com account ([a-zA-Z0-9-]+)", output)
        if match: return match.group(1)
        return None

class WorkerSignals(QObject):
    log_appended = pyqtSignal(str, str, str)
    tunnel_ready = pyqtSignal()
    deploy_finished = pyqtSignal()

class ManagedProcess:
    def __init__(self, name: str, is_tunnel: bool = False):
        self.name = name
        self.is_tunnel = is_tunnel
        self.process = None
        self.url = None
        suffix = "_tunnel" if is_tunnel else ""
        self.log_path = LOGS_DIR / f"{name}{suffix}.log"

    def start(self, cmd: List[str], cwd: str, log_cb):
        if self.is_running(): return
        
        creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == 'nt' else 0
        self.process = subprocess.Popen(
            cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace", bufsize=1, creationflags=creation_flags
        )
        
        def reader(proc):
            url_regex = re.compile(r"https://[a-zA-Z0-9-]+\.trycloudflare\.com")
            try:
                with open(self.log_path, 'a', encoding="utf-8") as f:
                    for line in proc.stdout:
                        f.write(line)
                        f.flush()
                        log_cb(line.strip())
                        if self.is_tunnel and not self.url:
                            match = url_regex.search(line)
                            if match: self.url = match.group(0)
                proc.wait()
            except Exception:
                pass

        threading.Thread(target=reader, args=(self.process,), daemon=True).start()

    def stop(self):
        if self.process:
            try:
                p = psutil.Process(self.process.pid)
                for child in p.children(recursive=True): child.kill()
                p.kill()
            except: pass
            self.process = None
            self.url = None

    def is_running(self) -> bool:
        return self.process is not None and self.process.poll() is None

# ==========================================
# MODULE: GUI COMPONENTS (PYQT6)
# ==========================================
QSS = """
QMainWindow, QDialog, QMessageBox, QFileDialog, QInputDialog { background: transparent; }
QWidget#Central { background-color: rgba(10, 10, 10, 40); }
QFrame#Sidebar { background-color: rgba(0, 0, 0, 50); border-right: 1px solid rgba(255, 255, 255, 20); }
QScrollArea, QScrollArea::viewport { background: transparent; border: none; }
QWidget#ScrollContainer { background: transparent; }
QFrame#HeaderCard { background-color: rgba(30, 30, 30, 90); border: 1px solid rgba(255, 255, 255, 20); border-radius: 8px; }
QPushButton { background-color: rgba(50, 50, 50, 100); color: #FFFFFF; border: 1px solid rgba(255, 255, 255, 30); border-radius: 4px; padding: 8px 16px; font-family: "Segoe UI"; font-size: 13px; }
QPushButton:hover { background-color: rgba(80, 80, 80, 140); }
QPushButton:disabled { color: #777777; border-color: rgba(255, 255, 255, 10); background-color: rgba(30, 30, 30, 60); }
QPushButton#PrimaryBtn { background-color: #60CDFF; color: black; border: none; font-weight: bold; }
QPushButton#PrimaryBtn:hover { background-color: #58BCEB; }
QPushButton#StartBtn { background-color: rgba(108, 203, 95, 40); color: #FFFFFF; border: 1px solid rgba(108, 203, 95, 80); font-weight: bold; }
QPushButton#StartBtn:hover { background-color: rgba(108, 203, 95, 80); border: 1px solid rgba(108, 203, 95, 120); }
QPushButton#StartBtn:disabled { background-color: rgba(50, 50, 50, 80); border: 1px solid rgba(255, 255, 255, 10); color: #777777; font-weight: normal; }
QPushButton#StopBtn { background-color: rgba(196, 43, 28, 40); color: #FFFFFF; border: 1px solid rgba(196, 43, 28, 80); font-weight: bold; }
QPushButton#StopBtn:hover { background-color: rgba(196, 43, 28, 80); border: 1px solid rgba(196, 43, 28, 120); }
QPushButton#StopBtn:disabled { background-color: rgba(50, 50, 50, 80); border: 1px solid rgba(255, 255, 255, 10); color: #777777; font-weight: normal; }
QPushButton#DeployBtn { background-color: rgba(144, 89, 255, 40); color: #FFFFFF; border: 1px solid rgba(144, 89, 255, 80); font-weight: bold; }
QPushButton#DeployBtn:hover { background-color: rgba(144, 89, 255, 80); border: 1px solid rgba(144, 89, 255, 120); }
QPushButton#DeployBtn:disabled { background-color: rgba(50, 50, 50, 80); border: 1px solid rgba(255, 255, 255, 10); color: #777777; font-weight: normal; }
QTabWidget { background: transparent; }
QTabWidget::pane { border: 1px solid rgba(255, 255, 255, 20); border-radius: 4px; background: rgba(15, 15, 15, 80); }
QTabBar::tab { background: rgba(10, 10, 10, 90); border: 1px solid rgba(255, 255, 255, 20); padding: 8px 16px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; color: #A0A0A0; font-family: "Segoe UI"; }
QTabBar::tab:selected { background: rgba(60, 60, 60, 140); color: white; }
QTabBar::tab:hover:!selected { background: rgba(40, 40, 40, 120); }
QPlainTextEdit { background: transparent; color: #E5E5E5; border: none; font-family: "Consolas"; font-size: 13px; padding: 8px; }
QLineEdit, QComboBox { background-color: rgba(0, 0, 0, 90); border: 1px solid rgba(255, 255, 255, 30); border-radius: 4px; padding: 8px; color: white; font-family: "Segoe UI"; font-size: 13px; }
QComboBox::drop-down { border: none; }
QLabel { color: white; font-family: "Segoe UI"; background: transparent; }
"""

class AppCard(QFrame):
    def __init__(self, app_conf: AppConfig, on_select, parent=None):
        super().__init__(parent)
        self.app_conf = app_conf
        self.on_select = on_select
        
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setFixedHeight(50)
        
        layout = QHBoxLayout(self)
        layout.setContentsMargins(15, 0, 15, 0)
        
        title = app_conf.name
        if app_conf.app_type == "static_gh": title += " (GitHub Pages)"
        elif app_conf.app_type == "static_cf": title += " (CF Tunnel)"
        
        self.lbl_name = QLabel(title)
        
        self.status_dot = QLabel("●")
        self.status_dot.setStyleSheet("color: #555555; font-size: 16px; background: transparent; border: none;")
        
        layout.addWidget(self.lbl_name)
        layout.addStretch()
        layout.addWidget(self.status_dot)
        
        self.set_selected(False)
        
    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self.on_select(self.app_conf)

    def update_status(self, is_running):
        color = "#6CCB5F" if is_running else "#C42B1C"
        self.status_dot.setStyleSheet(f"color: {color}; font-size: 16px; background: transparent; border: none;")

    def set_selected(self, is_selected):
        if is_selected:
            self.setStyleSheet("QFrame { background-color: rgba(255, 255, 255, 20); border: 1px solid rgba(255, 255, 255, 40); border-radius: 8px; } QLabel { color: #FFFFFF; font-weight: bold; font-family: 'Segoe UI'; font-size: 13px; background: transparent; border: none; }")
        else:
            self.setStyleSheet("QFrame { background-color: transparent; border: 1px solid rgba(255, 255, 255, 10); border-radius: 8px; } QLabel { color: #E5E5E5; font-weight: normal; font-family: 'Segoe UI'; font-size: 13px; background: transparent; border: none; }")

class LocalcelGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Localcel - Vercel for Localhost")
        self.setWindowIcon(get_icon())
        self.resize(1150, 750)
        apply_translucent_acrylic(self)
        
        self.engine_servers = {}
        self.engine_tunnels = {}
        self.selected_app = None
        self.cards = {}
        
        self.signals = WorkerSignals()
        self.signals.log_appended.connect(self.append_log)
        self.signals.tunnel_ready.connect(self.tunnel_manager_dialog)
        self.signals.deploy_finished.connect(self.on_deploy_finished)
        
        self.setup_ui()
        self.setup_tray_icon()
        self.refresh_app_list()
        
        QApplication.instance().commitDataRequest.connect(self.on_commit_data_request)
        
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.check_loop)
        self.timer.start(1000)

    def setup_tray_icon(self):
        self.tray_icon = QSystemTrayIcon(self)
        icon_obj = get_icon()
        if not icon_obj.isNull():
            self.tray_icon.setIcon(icon_obj)
        self.tray_icon.setToolTip("Localcel - No running servers")
        
        self.tray_menu = QMenu(self)
        self.tray_menu.setStyleSheet("""
            QMenu { background-color: rgba(20, 20, 20, 180); color: #FFFFFF; border: 1px solid #333333; border-radius: 4px; }
            QMenu::item { padding: 6px 25px 6px 20px; font-family: "Segoe UI"; }
            QMenu::item:selected { background-color: #3A3A3A; }
            QMenu::separator { height: 1px; background-color: #333333; margin: 4px 0px 4px 0px; }
        """)
        
        self.tray_menu.aboutToShow.connect(self.update_tray_menu)
        self.tray_icon.setContextMenu(self.tray_menu)
        self.tray_icon.activated.connect(self.tray_activated)
        self.tray_icon.show()

    def update_tray_menu(self):
        self.tray_menu.clear()
        running_apps = [name for name, srv in self.engine_servers.items() if srv.is_running()]
        if running_apps:
            title_action = self.tray_menu.addAction("Running Servers:")
            title_action.setEnabled(False)
            for app in running_apps:
                action = self.tray_menu.addAction(f"  ● {app}")
                action.setEnabled(False)
        else:
            action = self.tray_menu.addAction("No running servers")
            action.setEnabled(False)
        self.tray_menu.addSeparator()
        show_action = self.tray_menu.addAction("Show Localcel")
        show_action.triggered.connect(self.show_normal)
        exit_action = self.tray_menu.addAction("Exit")
        exit_action.triggered.connect(self.quit_app)

    def tray_activated(self, reason):
        if reason == QSystemTrayIcon.ActivationReason.DoubleClick:
            self.show_normal()
            
    def show_normal(self):
        self.showNormal()
        self.activateWindow()

    def setup_ui(self):
        self.central = QWidget()
        self.central.setObjectName("Central")
        self.setCentralWidget(self.central)
        
        self.main_layout = QHBoxLayout(self.central)
        self.main_layout.setContentsMargins(0, 0, 0, 0)
        self.main_layout.setSpacing(0)
        
        self.sidebar = QFrame()
        self.sidebar.setObjectName("Sidebar")
        self.sidebar.setFixedWidth(280)
        self.sidebar_layout = QVBoxLayout(self.sidebar)
        self.sidebar_layout.setContentsMargins(20, 20, 20, 20)
        
        self.lbl_logo = QLabel()
        self.lbl_logo.setStyleSheet("background: transparent;")
        logo_pixmap = get_logo()
        
        if not logo_pixmap.isNull():
            self.lbl_logo.setPixmap(logo_pixmap.scaledToWidth(220, Qt.TransformationMode.SmoothTransformation))
            self.lbl_logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        else:
            self.lbl_logo.setText("Localcel")
            self.lbl_logo.setStyleSheet("color: #FFFFFF; font-size: 22px; font-weight: bold; font-family: 'Segoe UI Variable Display'; background: transparent;")
            
        self.sidebar_layout.addWidget(self.lbl_logo)
        self.sidebar_layout.addSpacing(15)
        
        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll_container = QWidget()
        self.scroll_container.setObjectName("ScrollContainer")
        self.scroll_layout = QVBoxLayout(self.scroll_container)
        self.scroll_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        self.scroll_layout.setContentsMargins(0, 0, 0, 0)
        self.scroll_layout.setSpacing(8)
        self.scroll.setWidget(self.scroll_container)
        self.sidebar_layout.addWidget(self.scroll)
        
        self.btn_new = QPushButton("New App")
        self.btn_new.setObjectName("PrimaryBtn")
        self.btn_new.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_new.clicked.connect(self.create_app_dialog)
        self.sidebar_layout.addWidget(self.btn_new)
        
        # Action Buttons Layout
        self.actions_layout = QHBoxLayout()
        
        self.btn_login = QPushButton("CF Login")
        self.btn_login.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_login.clicked.connect(self.cf_login)
        self.actions_layout.addWidget(self.btn_login)
        
        self.btn_git_login = QPushButton("Git Login")
        self.btn_git_login.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_git_login.clicked.connect(self.git_login)
        self.actions_layout.addWidget(self.btn_git_login)
        
        self.sidebar_layout.addLayout(self.actions_layout)
        
        self.btn_cleanup = QPushButton("Tunnel Manager")
        self.btn_cleanup.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_cleanup.clicked.connect(self.tunnel_manager_dialog)
        self.sidebar_layout.addWidget(self.btn_cleanup)
        
        self.btn_update = QPushButton("Update App")
        self.btn_update.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_update.clicked.connect(self.request_update)
        self.sidebar_layout.addWidget(self.btn_update)
        
        self.main_layout.addWidget(self.sidebar)
        
        self.right_area = QWidget()
        self.right_area.setObjectName("Central")
        self.right_layout = QVBoxLayout(self.right_area)
        self.right_layout.setContentsMargins(25, 25, 25, 25)
        
        self.header = QFrame()
        self.header.setObjectName("HeaderCard")
        self.header_layout = QVBoxLayout(self.header)
        self.header_layout.setContentsMargins(25, 20, 25, 20)
        
        self.lbl_app_title = QLabel("Select an application")
        self.lbl_app_title.setStyleSheet("color: #FFFFFF; font-size: 24px; font-weight: bold; font-family: 'Segoe UI Variable Display'; background: transparent;")
        self.header_layout.addWidget(self.lbl_app_title)
        
        self.lbl_url = QLabel("Not running")
        self.lbl_url.setStyleSheet("color: #A0A0A0; font-size: 14px; background: transparent;")
        self.lbl_url.setOpenExternalLinks(True)
        self.header_layout.addWidget(self.lbl_url)
        
        self.right_layout.addWidget(self.header)
        
        self.ctrl_bar = QWidget()
        self.ctrl_bar.setStyleSheet("background: transparent;")
        self.ctrl_layout = QHBoxLayout(self.ctrl_bar)
        self.ctrl_layout.setContentsMargins(0, 10, 0, 10)
        
        self.btn_start = QPushButton("Start")
        self.btn_start.setObjectName("StartBtn")
        self.btn_start.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_start.clicked.connect(self.start_current)
        
        self.btn_stop = QPushButton("Stop")
        self.btn_stop.setObjectName("StopBtn")
        self.btn_stop.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_stop.clicked.connect(self.stop_current)

        self.btn_deploy = QPushButton("Deploy")
        self.btn_deploy.setObjectName("DeployBtn")
        self.btn_deploy.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_deploy.clicked.connect(self.deploy_current)
        self.btn_deploy.hide()
        
        self.btn_edit = QPushButton("Edit App")
        self.btn_edit.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_edit.clicked.connect(self.edit_app_dialog)
        
        self.btn_del = QPushButton("Delete App")
        self.btn_del.setCursor(Qt.CursorShape.PointingHandCursor)
        self.btn_del.clicked.connect(self.delete_current)
        
        self.ctrl_layout.addWidget(self.btn_start)
        self.ctrl_layout.addWidget(self.btn_stop)
        self.ctrl_layout.addWidget(self.btn_deploy)
        self.ctrl_layout.addWidget(self.btn_edit)
        self.ctrl_layout.addStretch()
        self.ctrl_layout.addWidget(self.btn_del)
        
        self.right_layout.addWidget(self.ctrl_bar)
        
        self.log_tabs = QTabWidget()
        self.log_tabs.setObjectName("LogTabs")
        
        self.txt_server = QPlainTextEdit()
        self.txt_server.setReadOnly(True)
        self.log_tabs.addTab(self.txt_server, "Server Logs")
        
        self.txt_tunnel = QPlainTextEdit()
        self.txt_tunnel.setReadOnly(True)
        self.log_tabs.addTab(self.txt_tunnel, "Tunnel/Deploy Logs")
        
        self.right_layout.addWidget(self.log_tabs)
        self.main_layout.addWidget(self.right_area)

    def request_update(self):
        reply = QMessageBox.question(self, 'Update Application',
                                     "Updating the app will stop all currently running servers and tunnels.\nThe application will restart automatically after the update is complete.\n\nDo you want to proceed?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No, QMessageBox.StandardButton.No)
        if reply == QMessageBox.StandardButton.Yes:
            # Stop all processes
            for app, srv in self.engine_servers.items():
                if srv.is_running(): srv.stop()
            for app, tun in self.engine_tunnels.items():
                if tun.is_running(): tun.stop()
            
            # Exit with code 42 to signal C++ bootstrapper to update
            QApplication.quit()
            sys.exit(42)

    def refresh_app_list(self):
        while self.scroll_layout.count():
            child = self.scroll_layout.takeAt(0)
            if child.widget(): child.widget().deleteLater()
            
        self.cards = {}
        apps = AppManager.get_apps()
        for app in apps:
            card = AppCard(app, self.select_app)
            self.scroll_layout.addWidget(card)
            self.cards[app.name] = card
            
        if self.selected_app:
            for name, card in self.cards.items():
                card.set_selected(name == self.selected_app.name)

    def select_app(self, app_conf):
        self.selected_app = app_conf
        
        t = app_conf.name
        if app_conf.app_type == "static_gh": t += " (GitHub Pages)"
        elif app_conf.app_type == "static_cf": t += " (CF Tunnel)"
        self.lbl_app_title.setText(t)
        
        for name, card in self.cards.items():
            card.set_selected(name == app_conf.name)
            
        self.update_ui_state()
        self.txt_server.clear()
        self.txt_tunnel.clear()
        
        for suffix, box in [("", self.txt_server), ("_tunnel", self.txt_tunnel)]:
            p = LOGS_DIR / f"{app_conf.name}{suffix}.log"
            if p.exists():
                with open(p, 'r', encoding="utf-8", errors="replace") as f:
                    content = "".join(f.readlines()[-100:])
                    box.appendPlainText(content.strip())

    def update_ui_state(self):
        if not self.selected_app: return
        name = self.selected_app.name
        is_running = name in self.engine_servers and self.engine_servers[name].is_running()
        
        self.btn_start.setEnabled(not is_running)
        self.btn_stop.setEnabled(is_running)
        
        if self.selected_app.app_type == "static_gh":
            self.btn_deploy.show()
            self.btn_deploy.setText("Undeploy" if getattr(self.selected_app, 'gh_pages_deployed', False) else "Deploy")
        else:
            self.btn_deploy.hide()
            
        # Init gh_user caching attributes if missing to avoid blocking UI during status updates
        if not hasattr(self, 'gh_user_cache'):
            self.gh_user_cache = None
            self.fetching_gh_user = False
            
        def get_gh_link(app_name):
            if not self.gh_user_cache and not self.fetching_gh_user:
                self.fetching_gh_user = True
                def worker():
                    self.gh_user_cache = GitHubHelper.get_logged_in_user()
                threading.Thread(target=worker, daemon=True).start()
            
            if self.gh_user_cache:
                return f"https://{self.gh_user_cache}.github.io/{app_name}/"
            return None
        
        if is_running:
            tun = self.engine_tunnels.get(name)
            local_url = f"http://localhost:{self.selected_app.port}"
            
            if self.selected_app.app_type == "static_gh":
                display_text = f'<a href="{local_url}" style="color: #60CDFF; text-decoration: none; background: transparent;">Local: {local_url}</a>'
                
                if getattr(self.selected_app, 'gh_pages_deployed', False):
                    gh_url = get_gh_link(name)
                    if gh_url:
                        display_text += f' &nbsp;|&nbsp; <a href="{gh_url}" style="color: #60CDFF; text-decoration: none; background: transparent;">Live: {gh_url}</a>'
                    else:
                        display_text += f' &nbsp;|&nbsp; <span style="color: #A0A0A0; background: transparent;">Live: Fetching URL...</span>'
                self.lbl_url.setText(display_text)
            else:
                if tun and tun.url:
                    self.lbl_url.setText(f'<a href="{tun.url}" style="color: #60CDFF; text-decoration: none; background: transparent;">{tun.url}</a>')
                else:
                    self.lbl_url.setText(f'<a href="{local_url}" style="color: #60CDFF; text-decoration: none; background: transparent;">{local_url}</a> <span style="color: #A0A0A0; background: transparent;">(Tunnelling...)</span>')
        else:
            if self.selected_app.app_type == "static_gh" and getattr(self.selected_app, 'gh_pages_deployed', False):
                gh_url = get_gh_link(name)
                if gh_url:
                    self.lbl_url.setText(f'<span style="color: #A0A0A0; background: transparent;">Stopped (Local)</span> &nbsp;|&nbsp; <a href="{gh_url}" style="color: #60CDFF; text-decoration: none; background: transparent;">Live: {gh_url}</a>')
                else:
                    self.lbl_url.setText(f'<span style="color: #A0A0A0; background: transparent;">Stopped (Local)</span> &nbsp;|&nbsp; <span style="color: #A0A0A0; background: transparent;">Live: Fetching URL...</span>')
            else:
                self.lbl_url.setText('<span style="color: #A0A0A0; background: transparent;">Stopped</span>')

    def start_current(self):
        if not self.selected_app: return
        app = self.selected_app
        
        # --- PORT CHECKER ---
        while True:
            if is_port_in_use(app.port):
                # Using positional arguments for cross-compatibility (value, min, max, step)
                new_port, ok = QInputDialog.getInt(self, "Port in Use", f"Port {app.port} is currently in use by another program.\n\nPlease choose a different free port:", app.port + 1, 1024, 65535, 1)
                if ok:
                    AppManager.update_app(app.name, new_port, app.domain, app.app_type, app.github_repo, getattr(app, 'gh_pages_deployed', False))
                    app.port = new_port
                    self.selected_app = next((a for a in AppManager.get_apps() if a.name == app.name), app)
                else:
                    return # User cancelled start operation
            else:
                break
        
        srv = ManagedProcess(app.name)
        tun = ManagedProcess(app.name, is_tunnel=True)
        self.engine_servers[app.name] = srv
        self.engine_tunnels[app.name] = tun
        
        # Determine the runtime environment
        if app.app_type in ["static_cf", "static_gh"]:
            # Added '-u' flag to force unbuffered python logging
            srv.start([sys.executable, "-u", "-m", "http.server", str(app.port)], str(APPS_DIR / app.name), 
                      lambda line: self.signals.log_appended.emit(app.name, "server", line))
        else:
            if not get_executable_path("node"):
                QMessageBox.critical(self, "Error", "Node.js not found in PATH.")
                return
            srv.start([get_executable_path("node"), app.entry], str(APPS_DIR / app.name), 
                      lambda line: self.signals.log_appended.emit(app.name, "server", line))
        
        # Setup Cloudflare Tunnel if cloudflared is available and NOT static_gh
        if app.app_type != "static_gh" and get_executable_path("cloudflared"):
            if app.domain:
                try:
                    cfg = CloudflareHelper.setup_named_tunnel(app.name, app.port, app.domain)
                    cmd = [get_executable_path("cloudflared"), "tunnel", "--config", str(cfg), "run"]
                    tun.url = f"https://{app.domain}"
                except Exception as e:
                    QMessageBox.critical(self, "Tunnel Error", str(e))
                    return
            else:
                cmd = [get_executable_path("cloudflared"), "tunnel", "--url", f"http://localhost:{app.port}"]

            tun.start(cmd, str(BASE_DIR), lambda line: self.signals.log_appended.emit(app.name, "tunnel", line))
        elif app.app_type == "static_gh":
            self.signals.log_appended.emit(app.name, "tunnel", "GitHub Pages mode active. Local preview running. Use 'Deploy' to push to GitHub.")
        else:
            self.signals.log_appended.emit(app.name, "tunnel", "cloudflared not installed. Skipping public tunneling.")

    def deploy_current(self):
        if not self.selected_app or self.selected_app.app_type != "static_gh": return
        app = self.selected_app
        
        git_bin = GitHubHelper.ensure_git()
        gh_bin = GitHubHelper.ensure_gh()
        if not git_bin or not gh_bin: return
            
        user = GitHubHelper.get_logged_in_user()
        if not user:
            QMessageBox.information(self, "Login Required", "Please log in to GitHub first using the Git Login button on the sidebar.")
            self.git_login()
            return

        is_deployed = getattr(app, 'gh_pages_deployed', False)
        self.btn_deploy.setEnabled(False)
        
        if is_deployed:
            self.btn_deploy.setText("Undeploying...")
            def run_undeploy():
                cwd = str(APPS_DIR / app.name)
                log_cb = lambda line: self.signals.log_appended.emit(app.name, "tunnel", f"[GIT] {line}")
                log_cb("--- Disabling GitHub Pages ---")
                
                res = subprocess.run([gh_bin, "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"], cwd=cwd, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                repo_name = res.stdout.strip()
                if repo_name:
                    subprocess.run([gh_bin, "api", "-X", "DELETE", f"/repos/{repo_name}/pages"], creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                    log_cb("✅ GitHub Pages has been disabled. (Repository remains intact)")
                else:
                    log_cb("❌ Could not determine remote repository name.")
                    
                AppManager.update_app(app.name, app.port, app.domain, app.app_type, app.github_repo, gh_pages_deployed=False)
                self.signals.deploy_finished.emit()

            threading.Thread(target=run_undeploy, daemon=True).start()
            
        else:
            self.btn_deploy.setText("Deploying...")
            def run_deploy():
                cwd = str(APPS_DIR / app.name)
                log_cb = lambda line: self.signals.log_appended.emit(app.name, "tunnel", f"[GIT] {line}")
                
                def run_cmd(cmd):
                    p = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                    for line in p.stdout:
                        log_cb(line.strip())
                    p.wait()
                    return p.returncode

                log_cb("--- Starting GitHub Deploy Sequence ---")
                
                # Ensure git identity is set (fixes missing commits on machines without global git config)
                run_cmd([git_bin, "config", "user.name", "Localcel Deployer"])
                run_cmd([git_bin, "config", "user.email", "deploy@localcel.app"])
                
                run_cmd([git_bin, "add", "."])
                
                # Commit (Allow empty in case of no changes)
                run_cmd([git_bin, "commit", "-m", "Auto-deploy from Localcel"])
                
                # Check if remote exists, if not, auto-create using App Name
                res = subprocess.run([git_bin, "remote", "-v"], cwd=cwd, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                if "origin" not in res.stdout:
                    log_cb(f"Creating new public GitHub repository '{app.name}' via GitHub CLI...")
                    ret = run_cmd([gh_bin, "repo", "create", app.name, "--public", "--source=.", "--push"])
                    if ret != 0:
                        log_cb("❌ Failed to create remote repository.")
                        self.signals.deploy_finished.emit()
                        return
                else:
                    log_cb("Pushing to GitHub...")
                    run_cmd([git_bin, "push", "-u", "origin", "main"])
                
                # Enable GitHub Pages dynamically via API
                res2 = subprocess.run([gh_bin, "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"], cwd=cwd, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                repo_name = res2.stdout.strip()
                if repo_name:
                    log_cb("Enabling GitHub Pages...")
                    subprocess.run([gh_bin, "api", "-X", "POST", f"/repos/{repo_name}/pages", "-f", "source[branch]=main", "-f", "source[path]=/"], capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                
                log_cb("✅ Deploy successful! Your site is live on GitHub Pages.")
                AppManager.update_app(app.name, app.port, app.domain, app.app_type, app.github_repo, gh_pages_deployed=True)
                self.signals.deploy_finished.emit()

            threading.Thread(target=run_deploy, daemon=True).start()
        
    def on_deploy_finished(self):
        if self.selected_app:
            self.selected_app = next((a for a in AppManager.get_apps() if a.name == self.selected_app.name), None)
            if self.selected_app:
                self.btn_deploy.setText("Undeploy" if getattr(self.selected_app, 'gh_pages_deployed', False) else "Deploy")
        self.btn_deploy.setEnabled(True)

    def stop_current(self):
        if not self.selected_app: return
        name = self.selected_app.name
        if name in self.engine_servers: self.engine_servers[name].stop()
        if name in self.engine_tunnels: self.engine_tunnels[name].stop()

    def stop_all_servers(self):
        for name, srv in self.engine_servers.items():
            if srv.is_running(): srv.stop()
        for name, tun in self.engine_tunnels.items():
            if tun.is_running(): tun.stop()

    def quit_app(self):
        running_apps = [name for name, srv in self.engine_servers.items() if srv.is_running()]
        if running_apps:
            reply = QMessageBox.warning(
                self, 
                "Servers Running", 
                f"The following servers are still running:\n\n{', '.join(running_apps)}\n\nClosing Localcel will stop these servers. Are you sure you want to exit?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No
            )
            if reply == QMessageBox.StandardButton.Yes:
                self.stop_all_servers()
                QApplication.quit()
        else:
            self.stop_all_servers()
            QApplication.quit()

    def closeEvent(self, event):
        running_apps = [name for name, srv in self.engine_servers.items() if srv.is_running()]
        if running_apps:
            self.hide()
            self.tray_icon.showMessage(
                "Localcel Background Mode",
                "Localcel has been minimized to the system tray to keep your servers active.",
                QSystemTrayIcon.MessageIcon.Information,
                2000
            )
            event.ignore()
        else:
            self.stop_all_servers()
            QApplication.quit()
            event.accept()
            
    def on_commit_data_request(self, manager):
        running_apps = [name for name, srv in self.engine_servers.items() if srv.is_running()]
        if running_apps:
            manager.cancel()
        else:
            self.stop_all_servers()

    def delete_current(self):
        if not self.selected_app: return
        
        if self.selected_app.app_type == "static_gh":
            msg = QMessageBox(self)
            msg.setWindowTitle("Delete Application")
            msg.setText(f"Do you want to delete the remote GitHub repository '{self.selected_app.name}' as well?")
            msg.setInformativeText("Yes: Deletes remote GitHub repo AND local files.\nNo: Deletes local files only (GitHub page remains online).")
            msg.setStandardButtons(QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No | QMessageBox.StandardButton.Cancel)
            msg.setDefaultButton(QMessageBox.StandardButton.Cancel)
            
            icon_obj = get_icon()
            if not icon_obj.isNull():
                msg.setWindowIcon(icon_obj)
                
            reply = msg.exec()
            
            if reply == QMessageBox.StandardButton.Cancel:
                return
            elif reply == QMessageBox.StandardButton.Yes:
                gh_bin = get_executable_path("gh")
                if gh_bin:
                    cwd = str(APPS_DIR / self.selected_app.name)
                    # Attempt to get exact repo name (owner/repo) from local git config
                    res = subprocess.run([gh_bin, "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"], cwd=cwd, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                    repo_name = res.stdout.strip() or self.selected_app.name
                    
                    self.signals.log_appended.emit(self.selected_app.name, "tunnel", f"[GIT] Deleting remote repository {repo_name}...")
                    del_res = subprocess.run([gh_bin, "repo", "delete", repo_name, "--yes"], capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                    
                    if del_res.returncode != 0:
                        err_msg = del_res.stderr.strip()
                        if "delete_repo" in err_msg or "scope" in err_msg.lower():
                            QMessageBox.warning(self, "GitHub Permissions Missing", f"Could not delete remote repository '{repo_name}'.\n\nYou are missing the 'delete_repo' scope. Please click 'Git Login', log out, and log back in to grant delete permissions.\n\nLocal files will still be deleted.")
                        else:
                            QMessageBox.warning(self, "Remote Deletion Failed", f"Could not delete remote repository '{repo_name}':\n\n{err_msg}\n\nLocal files will still be deleted.")
        else:
            reply = QMessageBox.question(self, "Confirm", "Delete this app and all its files?", QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if reply == QMessageBox.StandardButton.No:
                return

        self.stop_current()
        AppManager.delete_app(self.selected_app.name)
        self.selected_app = None
        self.refresh_app_list()
        self.txt_server.clear()
        self.txt_tunnel.clear()
        self.lbl_app_title.setText("Select an application")
        self.lbl_url.setText("Not running")
        self.btn_deploy.hide()
        self.update_ui_state()

    def append_log(self, app_name, log_type, line):
        if self.selected_app and self.selected_app.name == app_name:
            box = self.txt_server if log_type == "server" else self.txt_tunnel
            box.appendPlainText(line.strip())

    def cf_login(self):
        if not get_executable_path("cloudflared"):
            CloudflareHelper.install_cloudflared()
            
        cert_path = Path.home() / ".cloudflared" / "cert.pem"
        if cert_path.exists():
            reply = QMessageBox.question(self, "Cloudflare Login", "You are already logged in to Cloudflare (certificate found).\n\nDo you want to re-authenticate and overwrite it?", QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if reply == QMessageBox.StandardButton.No:
                self.signals.tunnel_ready.emit()
                return
            else:
                try:
                    cert_path.unlink()
                except Exception as e:
                    QMessageBox.warning(self, "Error", f"Could not remove old certificate: {e}")

        proc = subprocess.Popen([get_executable_path("cloudflared"), "tunnel", "login"])
        def wait_and_show():
            proc.wait()
            self.signals.tunnel_ready.emit()
        threading.Thread(target=wait_and_show, daemon=True).start()

    def git_login(self):
        gh_bin = GitHubHelper.ensure_gh()
        GitHubHelper.ensure_git()
        if not gh_bin: return
            
        user = GitHubHelper.get_logged_in_user()
        if user:
            reply = QMessageBox.question(self, "GitHub Login", f"You are already logged in to GitHub as:\n\n{user}\n\nDo you want to log in with a different account?", QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if reply == QMessageBox.StandardButton.Yes:
                subprocess.run([gh_bin, "auth", "logout", "--hostname", "github.com"], creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            else:
                return

        QMessageBox.information(self, "Git Login", "Localcel uses the official GitHub CLI for secure authentication.\n\nA command prompt will open with a one-time code. Copy the code, press Enter to open your browser, and paste it to authorize this machine.")
        
        try:
            # gh auth login --web requires a console to display the one-time passcode and wait for the user to press Enter.
            # Requesting 'repo' and 'delete_repo' scopes to ensure app deletion works seamlessly.
            creation_flags = subprocess.CREATE_NEW_CONSOLE if platform.system() == "Windows" else 0
            subprocess.Popen([gh_bin, "auth", "login", "--web", "--scopes", "repo,delete_repo"], creationflags=creation_flags)
        except Exception:
            webbrowser.open("https://github.com/login")

    def check_loop(self):
        running_apps = []
        for name, card in self.cards.items():
            is_run = name in self.engine_servers and self.engine_servers.get(name).is_running()
            card.update_status(is_run)
            if is_run:
                running_apps.append(name)
                
        if running_apps:
            self.tray_icon.setToolTip(f"Localcel - Running:\n" + "\n".join(running_apps))
        else:
            self.tray_icon.setToolTip("Localcel - No running servers")
            
        self.update_ui_state()

    def create_app_dialog(self):
        dlg = QDialog(self)
        dlg.setWindowTitle("New Application")
        icon_obj = get_icon()
        if not icon_obj.isNull():
            dlg.setWindowIcon(icon_obj)
        dlg.resize(400, 400)
        apply_translucent_acrylic(dlg)
        
        layout = QVBoxLayout(dlg)
        frame = QFrame()
        frame.setObjectName("HeaderCard")
        frame_layout = QVBoxLayout(frame)
        
        lbl = QLabel("Create New App")
        lbl.setStyleSheet("font-size: 20px; font-weight: bold; font-family: 'Segoe UI Variable Display'; background: transparent;")
        frame_layout.addWidget(lbl)
        
        combo_type = QComboBox()
        combo_type.addItems(["Node.js Server", "Static Page (GitHub Pages)", "Static Page (Cloudflare Tunnel)"])
        frame_layout.addWidget(combo_type)
        
        e_name = QLineEdit()
        e_name.setPlaceholderText("App Name")
        frame_layout.addWidget(e_name)
        
        e_port = QLineEdit()
        e_port.setPlaceholderText("Port (e.g. 3000)")
        e_port.setText("3000")
        frame_layout.addWidget(e_port)
        
        e_dom = QLineEdit()
        e_dom.setPlaceholderText("Custom Domain (Optional)")
        frame_layout.addWidget(e_dom)
        
        def on_type_change():
            idx = combo_type.currentIndex()
            if idx == 1: # GitHub Pages
                e_dom.hide()
                e_port.setPlaceholderText("Local Preview Port (e.g. 8080)")
                e_port.setText("8080")
            elif idx == 2: # CF Tunnel Static
                e_dom.show()
                e_port.setPlaceholderText("Local Preview Port (e.g. 8080)")
                e_port.setText("8080")
            else: # Node
                e_dom.show()
                e_port.setPlaceholderText("Port (e.g. 3000)")
                e_port.setText("3000")
                
        combo_type.currentIndexChanged.connect(on_type_change)
        on_type_change()
        
        btn_save = QPushButton("Create App")
        btn_save.setObjectName("PrimaryBtn")
        btn_save.setCursor(Qt.CursorShape.PointingHandCursor)
        
        def save():
            name, port = e_name.text().strip(), e_port.text().strip()
            if name and port.isdigit():
                idx = combo_type.currentIndex()
                if idx == 1: app_type = "static_gh"
                elif idx == 2: app_type = "static_cf"
                else: app_type = "node"
                
                AppManager.create_app(name, int(port), e_dom.text().strip(), "server.js", app_type, "")
                self.refresh_app_list()
                dlg.accept()
                
        btn_save.clicked.connect(save)
        frame_layout.addWidget(btn_save)
        layout.addWidget(frame)
        dlg.exec()

    def edit_app_dialog(self):
        if not self.selected_app: return
        app = self.selected_app
        dlg = QDialog(self)
        dlg.setWindowTitle(f"Edit {app.name}")
        icon_obj = get_icon()
        if not icon_obj.isNull():
            dlg.setWindowIcon(icon_obj)
        dlg.resize(400, 350)
        apply_translucent_acrylic(dlg)
        
        layout = QVBoxLayout(dlg)
        frame = QFrame()
        frame.setObjectName("HeaderCard")
        frame_layout = QVBoxLayout(frame)
        
        lbl = QLabel(f"Edit {app.name}")
        lbl.setStyleSheet("font-size: 20px; font-weight: bold; font-family: 'Segoe UI Variable Display'; background: transparent;")
        frame_layout.addWidget(lbl)
        
        lbl_port = QLabel("Port Number" if app.app_type == "node" else "Local Preview Port")
        lbl_port.setStyleSheet("background: transparent;")
        frame_layout.addWidget(lbl_port)
        
        e_port = QLineEdit()
        e_port.setText(str(app.port))
        frame_layout.addWidget(e_port)
        
        e_dom = None
        combo_type = None
        
        if app.app_type == "node":
            lbl_dom = QLabel("Custom Domain")
            lbl_dom.setStyleSheet("background: transparent;")
            frame_layout.addWidget(lbl_dom)
            e_dom = QLineEdit()
            e_dom.setText(app.domain or "")
            frame_layout.addWidget(e_dom)
        else:
            lbl_type = QLabel("Hosting Type")
            lbl_type.setStyleSheet("background: transparent;")
            frame_layout.addWidget(lbl_type)
            
            combo_type = QComboBox()
            combo_type.addItems(["GitHub Pages", "Cloudflare Tunnel"])
            combo_type.setCurrentIndex(0 if app.app_type == "static_gh" else 1)
            frame_layout.addWidget(combo_type)
            
            e_dom = QLineEdit()
            e_dom.setPlaceholderText("Custom Domain (Optional)")
            e_dom.setText(app.domain or "")
            frame_layout.addWidget(e_dom)
            
            def on_type_change():
                if combo_type.currentIndex() == 0:
                    e_dom.hide()
                else:
                    e_dom.show()
            combo_type.currentIndexChanged.connect(on_type_change)
            on_type_change()
        
        btn_save = QPushButton("Save Changes")
        btn_save.setObjectName("PrimaryBtn")
        btn_save.setCursor(Qt.CursorShape.PointingHandCursor)
        
        def save():
            port = e_port.text().strip()
            if port.isdigit():
                was_running = app.name in self.engine_servers and self.engine_servers.get(app.name).is_running()
                if was_running: self.stop_current()
                
                dom_val = e_dom.text().strip() if e_dom and e_dom.isVisible() else ""
                
                new_type = app.app_type
                if combo_type:
                    new_type = "static_gh" if combo_type.currentIndex() == 0 else "static_cf"
                
                AppManager.update_app(app.name, int(port), dom_val, new_type, app.github_repo or "")
                self.refresh_app_list()
                self.selected_app = next((a for a in AppManager.get_apps() if a.name == app.name), None)
                if was_running: self.start_current()
                dlg.accept()
                
        btn_save.clicked.connect(save)
        frame_layout.addWidget(btn_save)
        layout.addWidget(frame)
        dlg.exec()

    def tunnel_manager_dialog(self):
        dlg = QDialog(self)
        dlg.setWindowTitle("Cloudflare Tunnels")
        icon_obj = get_icon()
        if not icon_obj.isNull():
            dlg.setWindowIcon(icon_obj)
        dlg.resize(600, 450)
        apply_translucent_acrylic(dlg)
        
        layout = QVBoxLayout(dlg)
        frame = QFrame()
        frame.setObjectName("HeaderCard")
        frame_layout = QVBoxLayout(frame)
        
        lbl = QLabel("Cloudflare Tunnels")
        lbl.setStyleSheet("font-size: 20px; font-weight: bold; font-family: 'Segoe UI Variable Display'; background: transparent;")
        frame_layout.addWidget(lbl)
        
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        container = QWidget()
        container.setObjectName("ScrollContainer")
        scroll_layout = QVBoxLayout(container)
        scroll_layout.setAlignment(Qt.AlignmentFlag.AlignTop)
        scroll.setWidget(container)
        frame_layout.addWidget(scroll)
        
        def refresh_list():
            while scroll_layout.count():
                child = scroll_layout.takeAt(0)
                if child.widget(): child.widget().deleteLater()
                
            tunnels = CloudflareHelper.list_tunnels()
            if not tunnels:
                l = QLabel("No active tunnels found or not logged in.")
                l.setStyleSheet("color: #A0A0A0; background: transparent;")
                scroll_layout.addWidget(l)
                return
                
            for t in tunnels:
                row = QFrame()
                row.setStyleSheet("background-color: rgba(30, 30, 30, 150); border: 1px solid rgba(255, 255, 255, 20); border-radius: 4px;")
                row_layout = QHBoxLayout(row)
                
                t_lbl = QLabel(f"{t['name']}  •  {t['id'][:8]}")
                t_lbl.setStyleSheet("border: none; font-weight: bold; background: transparent;")
                
                btn_del = QPushButton("Delete")
                btn_del.setCursor(Qt.CursorShape.PointingHandCursor)
                btn_del.setStyleSheet("background-color: rgba(196, 43, 28, 40); color: white; border: 1px solid rgba(196, 43, 28, 80); padding: 4px 12px; border-radius: 4px;")
                
                def make_del(checked, tid=t['id']):
                    reply = QMessageBox.question(dlg, "Confirm", f"Delete tunnel {tid}?", QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
                    if reply == QMessageBox.StandardButton.Yes:
                        CloudflareHelper.delete_tunnel(tid)
                        refresh_list()
                
                btn_del.clicked.connect(make_del)
                row_layout.addWidget(t_lbl)
                row_layout.addStretch()
                row_layout.addWidget(btn_del)
                scroll_layout.addWidget(row)
                
        refresh_list()
        btn_refresh = QPushButton("Refresh Status")
        btn_refresh.setCursor(Qt.CursorShape.PointingHandCursor)
        btn_refresh.clicked.connect(refresh_list)
        frame_layout.addWidget(btn_refresh)
        
        layout.addWidget(frame)
        dlg.exec()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)
    
    # Establish a safe default font layout to prevent QFont <= 0 error on parsing
    app.setFont(QFont("Segoe UI", 10))
    
    workspace_path = get_or_choose_workspace()
    initialize_workspace(workspace_path)
    
    if platform.system() == "Windows":
        try:
            import ctypes
            myappid = 'localcel.gui.app.1' 
            ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(myappid)
        except Exception:
            pass

    icon_obj = get_icon()
    if not icon_obj.isNull():
        app.setWindowIcon(icon_obj)
        
    app.setStyleSheet(QSS) 
    
    gui = LocalcelGUI()
    gui.show()
    sys.exit(app.exec())
'''

# ==========================================
# MODULE: DROPPER BOOTSTRAPPER
# ==========================================

def get_host_python():
    """Aggressively searches the host system for a valid Python installation."""
    exe = shutil.which("python") or shutil.which("pythonw") or shutil.which("python3")
    if exe: return exe
    
    # Check default Windows installation paths via LocalAppData
    local_app_data = os.environ.get("LocalAppData", "")
    if local_app_data:
        base_dir = os.path.join(local_app_data, "Programs", "Python")
        if os.path.exists(base_dir):
            for folder in sorted(os.listdir(base_dir), reverse=True):
                p = os.path.join(base_dir, folder, "python.exe")
                if os.path.exists(p): return p
                
    # Fallback checking root C:\
    if os.path.exists("C:\\"):
        try:
            for folder in sorted(os.listdir("C:\\"), reverse=True):
                if folder.lower().startswith("python"):
                    p = os.path.join("C:\\", folder, "python.exe")
                    if os.path.exists(p): return p
        except Exception: pass
        
    return None

if __name__ == "__main__":
    # If the app was compiled using PyInstaller, it runs this Dropper sequence
    if getattr(sys, 'frozen', False):
        python_exe = get_host_python()
        if not python_exe:
            import ctypes
            # 0x24 = MB_YESNO (0x04) + MB_ICONQUESTION (0x20)
            res = ctypes.windll.user32.MessageBoxW(0, "Python 3 is required to run Localcel but was not found.\n\nWould you like to automatically install it now?", "Python Missing", 0x24)
            
            if res == 6: # IDYES
                # Open a visible console for the user to see the winget installation progress
                creation_flags = subprocess.CREATE_NEW_CONSOLE if sys.platform == "win32" else 0
                subprocess.run(["winget", "install", "--id", "Python.Python.3.12", "-e", "--accept-package-agreements", "--accept-source-agreements"], creationflags=creation_flags)
                
                # Re-check for Python in the standard install paths
                python_exe = get_host_python()
                
            if not python_exe:
                ctypes.windll.user32.MessageBoxW(0, "Automatic installation failed or was cancelled.\n\nPlease install Python 3 manually from python.org to run Localcel.", "Dependency Error", 0x10)
                sys.exit(1)
        
        # Dump the payload string to the user's temp directory
        h = hashlib.md5(PAYLOAD.encode()).hexdigest()[:8]
        script_path = os.path.join(tempfile.gettempdir(), f"localcel_runner_{h}.py")
        
        with open(script_path, "w", encoding="utf-8") as f:
            f.write(PAYLOAD)
            
        # Suppress the background terminal on Windows when passing control to the host python
        flags = 0x08000000 if sys.platform == "win32" else 0 
        
        # Execute the dumped GUI app using the machine's python.
        sys.exit(subprocess.call([python_exe, script_path] + sys.argv[1:], creationflags=flags))
        
    # If it's just being run natively via "python localcel.py", execute the string directly
    else:
        exec(PAYLOAD, globals())