# FinceptTerminal Installer — run as Administrator
# Right-click → "Run with PowerShell"

$AppName = "FinceptTerminal"
$TargetDir = "$env:LOCALAPPDATA\$AppName"
$Desktop = [Environment]::GetFolderPath("Desktop")
$StartMenu = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\$AppName"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Installing $AppName..." -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# 1. Create target directory
Write-Host "[1/5] Creating install directory..." -NoNewline
New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
Write-Host " OK" -ForegroundColor Green

# 2. Copy files
Write-Host "[2/5] Copying files..." -NoNewline
Copy-Item "$ScriptDir\*" $TargetDir -Recurse -Force
Write-Host " OK" -ForegroundColor Green

# 3. Create Start Menu shortcut
Write-Host "[3/5] Creating Start Menu shortcut..." -NoNewline
New-Item -ItemType Directory -Force -Path $StartMenu | Out-Null
$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("$StartMenu\$AppName.lnk")
$Shortcut.TargetPath = "$TargetDir\FinceptTerminal.exe"
$Shortcut.WorkingDirectory = $TargetDir
$Shortcut.Description = "Fincept Terminal - AI-Powered Trading"
$Shortcut.Save()
Write-Host " OK" -ForegroundColor Green

# 4. Create Local Mode shortcut
$LocalShortcut = $WshShell.CreateShortcut("$StartMenu\$AppName (Local MT5).lnk")
$LocalShortcut.TargetPath = "$TargetDir\start_local.bat"
$LocalShortcut.WorkingDirectory = $TargetDir
$LocalShortcut.Description = "Start FinceptTerminal with local MT5 worker"
$LocalShortcut.Save()
Write-Host " OK" -ForegroundColor Green

# 5. Create Desktop shortcut
Write-Host "[4/5] Creating Desktop shortcut..." -NoNewline
$DesktopShortcut = $WshShell.CreateShortcut("$Desktop\$AppName.lnk")
$DesktopShortcut.TargetPath = "$TargetDir\FinceptTerminal.exe"
$DesktopShortcut.WorkingDirectory = $TargetDir
$DesktopShortcut.Description = "Fincept Terminal - AI-Powered Trading"
$DesktopShortcut.Save()
Write-Host " OK" -ForegroundColor Green

# 6. Add PATH for Python scripts
Write-Host "[5/5] Registering Python scripts..." -NoNewline
$env:Path += ";$TargetDir"
[Environment]::SetEnvironmentVariable("Path", "$env:Path;$TargetDir", "User")
Write-Host " OK" -ForegroundColor Green

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  Installation Complete!" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""
Write-Host "Launch from Desktop shortcut: $AppName" -ForegroundColor Yellow
Write-Host "For local MT5 trading: Start Menu → '$AppName (Local MT5)'" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press any key to launch the app..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

# Launch
Start-Process "$TargetDir\FinceptTerminal.exe"
