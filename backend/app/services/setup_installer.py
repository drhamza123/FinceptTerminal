"""FinceptTerminal Setup — compiled to FinceptTerminal_Setup.exe via PyInstaller"""
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

APP_NAME = "AI Stock Guardian"
ZIP_NAME = "FinceptTerminal_v2.zip"


def get_zip_path() -> str:
    """Find the zip file bundled with this installer (same directory)."""
    base = Path(getattr(sys, '_MEIPASS', Path(__file__).parent))
    for f in base.glob("FinceptTerminal*.zip"):
        return str(f)
    # Fallback: look in current dir
    for f in Path(".").glob("FinceptTerminal*.zip"):
        return str(f)
    return ""


def main():
    print("========================================")
    print(f"  Installing {APP_NAME}...")
    print("========================================")
    print()

    # Find zip
    zip_path = get_zip_path()
    if not zip_path:
        print("ERROR: FinceptTerminal_v2.zip not found!")
        print("Place this installer in the same folder as the zip file.")
        input("Press Enter to exit...")
        return 1

    # Target directory
    target_dir = os.path.join(os.environ.get("LOCALAPPDATA", os.path.expanduser("~")), APP_NAME.replace(" ", ""))
    os.makedirs(target_dir, exist_ok=True)

    # Extract zip
    print(f"Extracting to {target_dir}...")
    with zipfile.ZipFile(zip_path) as z:
        z.extractall(target_dir)
    print(f"  {len(os.listdir(target_dir))} files extracted")

    # Create shortcuts
    desktop = os.path.join(os.path.expanduser("~"), "Desktop")
    start_menu = os.path.join(os.environ.get("APPDATA", ""),
                              "Microsoft", "Windows", "Start Menu", "Programs", APP_NAME)
    os.makedirs(start_menu, exist_ok=True)

    exe_path = os.path.join(target_dir, "FinceptTerminal.exe")

    try:
        import winshell
        # Desktop shortcut
        winshell.CreateShortcut(
            Path(os.path.join(desktop, f"{APP_NAME}.lnk")),
            target=exe_path,
            working_dir=target_dir,
            description=f"{APP_NAME} - AI-Powered Trading Terminal"
        )
        # Start menu shortcut
        winshell.CreateShortcut(
            Path(os.path.join(start_menu, f"{APP_NAME}.lnk")),
            target=exe_path,
            working_dir=target_dir,
            description=f"{APP_NAME} - AI-Powered Trading Terminal"
        )
        print("  Shortcuts created")
    except ImportError:
        print("  (Shortcuts skipped - winshell not available)")

    print()
    print("========================================")
    print("  Installation Complete!")
    print("========================================")
    print(f"  {APP_NAME} installed to:")
    print(f"    {target_dir}")
    print()
    print("  Launch from Desktop shortcut or Start Menu")
    print()

    # Launch the app
    print("Launching...")
    subprocess.Popen([exe_path], cwd=target_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
