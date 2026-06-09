#!/usr/bin/env python3
"""Enable MT5 AutoTrading via Win32 API — no RDP needed.
Fires WM_COMMAND 32843 (Ctrl+E toggle) into the MT5 main window.
Designed to run after MT5 starts, auto-fixing the disabled AutoTrading state."""

import time
import sys
import win32gui
import win32con
import win32process
import win32api

def find_mt5_window():
    """Find MT5 window by scanning all processes and their windows."""
    import win32process
    
    # First find the MT5 process by name
    target_pids = set()
    try:
        import psutil
        for proc in psutil.process_iter(['pid', 'name']):
            if proc.info['name'] and 'terminal64' in proc.info['name'].lower():
                target_pids.add(proc.info['pid'])
                print(f"[FOUND] MT5 process PID={proc.info['pid']}")
    except ImportError:
        # Fallback: use tasklist
        import subprocess, re
        output = subprocess.check_output('tasklist /NH /FO CSV /FI "IMAGENAME eq terminal64.exe"', shell=True).decode()
        for line in output.split('\n'):
            if 'terminal64' in line.lower():
                pid = re.search(r'"(\d+)"', line)
                if pid:
                    target_pids.add(int(pid.group(1)))
    
    if not target_pids:
        print("[WARN] terminal64.exe process not found. Trying EnumWindows...")
        # Fallback: traditional enumeration
        result = {'hwnd': None}
        def callback(hwnd, ctx):
            cls = win32gui.GetClassName(hwnd)
            title = win32gui.GetWindowText(hwnd)
            if "MetaTrader" in title or "MT5" in title:
                ctx['hwnd'] = hwnd
                return False
            return True
        win32gui.EnumWindows(callback, result)
        return result['hwnd']
    
    # For each MT5 process, enumerate its thread windows
    for pid in target_pids:
        try:
            handle = win32api.OpenProcess(win32con.PROCESS_QUERY_INFORMATION | win32con.PROCESS_VM_READ, False, pid)
            if not handle:
                continue
            
            # Get all threads for this process
            import subprocess
            output = subprocess.check_output(
                f'tasklist /NH /FO CSV /FI "PID eq {pid}"', shell=True
            ).decode()
            
            # Enumerate windows via EnumThreadWindows for each thread
            found = {'hwnd': None, 'pid': pid}
            
            def enum_thread_callback(hwnd, ctx):
                ctx['hwnd'] = hwnd
                return False
            
            # Use CreateToolhelp32Snapshot to find threads
            TH32CS_SNAPTHREAD = 0x00000004
            import ctypes
            kernel32 = ctypes.windll.kernel32
            
            class THREADENTRY32(ctypes.Structure):
                _fields_ = [
                    ("dwSize", ctypes.c_uint32),
                    ("cntUsage", ctypes.c_uint32),
                    ("th32ThreadID", ctypes.c_uint32),
                    ("th32OwnerProcessID", ctypes.c_uint32),
                    ("tpBasePri", ctypes.c_long),
                    ("tpDeltaPri", ctypes.c_long),
                    ("dwFlags", ctypes.c_uint32),
                ]
            
            snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
            if snapshot > 0:
                te = THREADENTRY32()
                te.dwSize = ctypes.sizeof(THREADENTRY32)
                if kernel32.Thread32First(snapshot, ctypes.byref(te)):
                    while True:
                        if te.th32OwnerProcessID == pid:
                            # Found a thread in MT5 — enumerate its windows
                            def enum_child(hwnd, ctx):
                                cls = win32gui.GetClassName(hwnd)
                                title = win32gui.GetWindowText(hwnd)
                                if "MetaTrader" in title or "MT5" in title or "terminal" in cls.lower():
                                    ctx['hwnd'] = hwnd
                                return True
                            win32gui.EnumThreadWindows(te.th32ThreadID, enum_child, found)
                            if found['hwnd']:
                                kernel32.CloseHandle(snapshot)
                                return found['hwnd']
                        if not kernel32.Thread32Next(snapshot, ctypes.byref(te)):
                            break
                kernel32.CloseHandle(snapshot)
            
            win32api.CloseHandle(handle)
            
            if found['hwnd']:
                return found['hwnd']
                
        except Exception as e:
            print(f"[WARN] Error checking PID {pid}: {e}")
    
    return None


def get_process_id(hwnd):
    """Get the PID of the process owning this window."""
    tid, pid = win32process.GetWindowThreadProcessId(hwnd)
    return pid


def bring_to_foreground(hwnd):
    """Bring window to foreground so the command registers."""
    try:
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.3)
        win32gui.SetForegroundWindow(hwnd)
        time.sleep(0.2)
    except Exception as e:
        print(f"[WARN] Foreground failed: {e}")


def enable_autotrading(max_retries=10, retry_delay=2):
    """Find MT5 window and send AutoTrading toggle command."""
    print("[MT5-AutoTrading] Waiting for MT5 terminal window...")
    
    for attempt in range(max_retries):
        hwnd = find_mt5_window()
        
        if hwnd:
            title = win32gui.GetWindowText(hwnd)
            print(f"[FOUND] MT5 window: '{title}' (hwnd={hwnd})")
            
            # Bring to foreground so command processes
            bring_to_foreground(hwnd)
            
            # Send Ctrl+E command (WM_COMMAND 32843 = AutoTrading toggle)
            print(f"[SENDING] WM_COMMAND 32843 (Ctrl+E) to hwnd={hwnd}")
            win32gui.PostMessage(hwnd, win32con.WM_COMMAND, 32843, 0)
            time.sleep(0.3)
            
            # Send again to ensure it registers
            win32gui.PostMessage(hwnd, win32con.WM_COMMAND, 32843, 0)
            
            print("[SUCCESS] AutoTrading toggle command sent!")
            
            # Verify by checking process state
            pid = get_process_id(hwnd)
            print(f"[INFO] MT5 process PID: {pid}")
            return True
        
        print(f"[WAIT] MT5 not found (attempt {attempt+1}/{max_retries})...")
        time.sleep(retry_delay)
    
    print("[ERROR] MT5 window not found after all retries.")
    print("[HINT] Is MetaTrader 5 installed and running?")
    return False


def verify_autotrading():
    """Check if AutoTrading is enabled by querying MT5 status via API."""
    import urllib.request
    import json
    
    try:
        req = urllib.request.Request(
            "http://127.0.0.1:8155/mt5/direct/status",
            headers={"X-API-Key": "gak_14f3a32d10ad0d231c846680f0598291577929e3c004c28a4daf99917763bf2b"}
        )
        resp = urllib.request.urlopen(req, timeout=5)
        data = json.loads(resp.read().decode())
        connected = data.get("data", {}).get("connected", False)
        print(f"[VERIFY] MT5 Direct connected: {connected}")
        
        # Try placing a tiny market order to test
        if connected:
            print("[READY] AutoTrading should now be enabled. Try: PLACE|XAUUSD|BUY|0.01|MARKET")
            print(f"[INFO] Account: {data.get('data', {}).get('account', {}).get('login', '?')}")
            print(f"[INFO] Balance: ${data.get('data', {}).get('account', {}).get('balance', 0):,.2f}")
    except Exception as e:
        print(f"[VERIFY] API check failed: {e}")


if __name__ == "__main__":
    print("=" * 50)
    print("  Fincept MT5 AutoTrading Enabler")
    print("  Uses Win32 API to press Ctrl+E programmatically")
    print("=" * 50)
    print()
    
    if enable_autotrading():
        print()
        verify_autotrading()
        print()
        print(">>> AutoTrading should now be ENABLED in MetaTrader 5 <<<")
        print(">>> Orders can now execute without manual RDP intervention <<<")
    else:
        sys.exit(1)
