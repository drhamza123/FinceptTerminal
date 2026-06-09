# AI Stock Guardian - Windows VPS Production Deploy Script
# Usage:
#   .\deploy-vps.ps1 -Domain trading.example.com
#   .\deploy-vps.ps1 -Domain trading.example.com -Mt5ConfigPath "C:\MT5\startup.ini"
#
# Must be run in PowerShell as Administrator.
# Do not put broker passwords in this repository. Keep MT5 startup.ini only on the VPS.

param (
    [string]$AppDir = "C:\opt\guardian",
    [string]$Domain = "",
    [string]$NssmPath = "",
    [string]$CaddyPath = "",
    [string]$Mt5TerminalPath = "",
    [string]$Mt5ConfigPath = ""
)

function Write-Ok { Write-Host "  OK" -ForegroundColor Green }
function Write-Warn($msg) { Write-Host "  WARNING: $msg" -ForegroundColor Yellow }
function Write-Fail($msg) { Write-Host "  FAILED: $msg" -ForegroundColor Red; exit 1 }

function Resolve-ToolPath($ProvidedPath, $ToolName) {
    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ProvidedPath)) { $candidates += $ProvidedPath }
    $candidates += "$AppDir\$ToolName"
    $candidates += "$ScriptDir\$ToolName"

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    }

    $cmd = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return ""
}

function Install-NssmService($ServiceName, $Executable, $Arguments, $WorkingDir, $StdoutLog, $StderrLog, $Description) {
    & $NssmExe stop $ServiceName 2>$null | Out-Null
    & $NssmExe remove $ServiceName confirm 2>$null | Out-Null

    & $NssmExe install $ServiceName $Executable $Arguments | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Fail "NSSM failed to install $ServiceName." }

    & $NssmExe set $ServiceName AppDirectory $WorkingDir | Out-Null
    & $NssmExe set $ServiceName DisplayName $ServiceName | Out-Null
    & $NssmExe set $ServiceName Description $Description | Out-Null
    & $NssmExe set $ServiceName Start SERVICE_AUTO_START | Out-Null
    & $NssmExe set $ServiceName AppStdout $StdoutLog | Out-Null
    & $NssmExe set $ServiceName AppStderr $StderrLog | Out-Null
    & $NssmExe set $ServiceName AppRotateFiles 1 | Out-Null
    & $NssmExe set $ServiceName AppRotateOnline 1 | Out-Null
    & $NssmExe set $ServiceName AppRotateBytes 10485760 | Out-Null
    & $NssmExe set $ServiceName AppThrottle 1500 | Out-Null
    & $NssmExe set $ServiceName AppExit Default Restart | Out-Null
    & $NssmExe start $ServiceName | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Fail "NSSM failed to start $ServiceName." }
}

function Resolve-Mt5Terminal {
    if (-not [string]::IsNullOrWhiteSpace($Mt5TerminalPath) -and (Test-Path $Mt5TerminalPath)) {
        return (Resolve-Path $Mt5TerminalPath).Path
    }

    $candidates = @(
        "C:\Program Files\MetaTrader 5\terminal64.exe",
        "C:\Program Files\MetaTrader 5\terminal.exe",
        "C:\Program Files (x86)\MetaTrader 5\terminal64.exe",
        "C:\Program Files (x86)\MetaTrader 5\terminal.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }

    return ""
}

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Fail "Run as Administrator: right-click PowerShell and choose Run as Administrator."
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = if (Test-Path "$ScriptDir\backend") { $ScriptDir } else { (Resolve-Path "$ScriptDir\..").Path }
$Domain = $Domain.Trim()
$UseHttps = -not [string]::IsNullOrWhiteSpace($Domain)

Write-Host ""
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host "  AI Stock Guardian - Windows VPS Production Deploy" -ForegroundColor Cyan
Write-Host "====================================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "[1/8] Checking Python installation..."
try {
    $pyVersion = & python --version 2>&1
    if (-not $pyVersion -match "Python 3") {
        Write-Fail "Python 3 is missing. Install Python 3.11 and enable Add to PATH."
    }
    Write-Ok
} catch {
    Write-Fail "Python is not installed or not in PATH."
}

Write-Host "[2/8] Checking NSSM..."
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null
$NssmExe = Resolve-ToolPath $NssmPath "nssm.exe"
if ([string]::IsNullOrWhiteSpace($NssmExe)) {
    Write-Fail "nssm.exe not found. Place nssm.exe in $AppDir, beside this script, or pass -NssmPath."
}
Write-Host "  NSSM: $NssmExe"
Write-Ok

Write-Host "[3/8] Deploying backend to $AppDir..."
if (-not (Test-Path "$RepoRoot\backend")) {
    Write-Fail "Cannot find 'backend' folder near $ScriptDir. Run this script from the repo root or scripts folder."
}
Copy-Item -Path "$RepoRoot\backend\*" -Destination $AppDir -Recurse -Force
New-Item -ItemType Directory -Force -Path "$AppDir\logs" | Out-Null
New-Item -ItemType Directory -Force -Path "$AppDir\data" | Out-Null
Write-Ok

Write-Host "[4/8] Creating virtual environment and installing dependencies..."
$VenvPython = "$AppDir\venv\Scripts\python.exe"
$VenvPip = "$AppDir\venv\Scripts\pip.exe"

if (-not (Test-Path $VenvPython)) {
    & python -m venv "$AppDir\venv"
    if ($LASTEXITCODE -ne 0) { Write-Fail "Failed to create venv." }
}

& $VenvPip install --quiet --upgrade pip
& $VenvPip install --quiet -r "$AppDir\requirements.txt"
if ($LASTEXITCODE -ne 0) { Write-Fail "Failed to install requirements." }
Write-Ok

Write-Host "[5/8] Configuring environment (.env)..."
$EnvFile = "$AppDir\.env"
if (-not (Test-Path $EnvFile)) {
    $chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    $jwtSecret = -join ((1..64) | ForEach-Object { $chars[(Get-Random -Maximum $chars.Length)] })

    $envContent = @"
LLM_PROVIDER_API_KEY=ollama
LLM_PROVIDER_BASE_URL=http://localhost:11434/v1
LLM_DEFAULT_MODEL=qwen2.5-coder:7b
DATABASE_URL=sqlite+aiosqlite:///./data/guardian.db
JWT_SECRET_KEY=$jwtSecret
FRED_API_KEY=
MT5_DIRECT_ENABLED=1
MT5_DIRECT_MAGIC=100000
MT5_DIRECT_DEVIATION=20
MT5_DIRECT_FILLING=FOK
"@
    Set-Content -Path $EnvFile -Value $envContent -Encoding UTF8
    Write-Warn "Created $EnvFile. Edit it to add API keys before production use."
} else {
    Write-Host "  $EnvFile already exists; leaving it unchanged."
}
Write-Ok

Write-Host "[6/8] Installing GuardianAPI NSSM service..."
Install-NssmService `
    -ServiceName "GuardianAPI" `
    -Executable $VenvPython `
    -Arguments "run.py" `
    -WorkingDir $AppDir `
    -StdoutLog "$AppDir\logs\guardian-api.out.log" `
    -StderrLog "$AppDir\logs\guardian-api.err.log" `
    -Description "AI Stock Guardian FastAPI backend on localhost:8150"
Start-Sleep -Seconds 3
Write-Ok

Write-Host "[7/8] Configuring HTTPS reverse proxy..."
if ($UseHttps) {
    $CaddyExe = Resolve-ToolPath $CaddyPath "caddy.exe"
    if ([string]::IsNullOrWhiteSpace($CaddyExe)) {
        Write-Fail "caddy.exe not found. Place caddy.exe in $AppDir, beside this script, or pass -CaddyPath."
    }

    $Caddyfile = "$AppDir\Caddyfile"
    $caddyContent = @"
$Domain {
    encode zstd gzip

    header {
        Strict-Transport-Security "max-age=31536000; includeSubDomains"
        X-Content-Type-Options "nosniff"
        X-Frame-Options "DENY"
        Referrer-Policy "no-referrer"
    }

    reverse_proxy 127.0.0.1:8150
}
"@
    Set-Content -Path $Caddyfile -Value $caddyContent -Encoding UTF8

    Install-NssmService `
        -ServiceName "GuardianCaddy" `
        -Executable $CaddyExe `
        -Arguments "run --config `"$Caddyfile`" --adapter caddyfile" `
        -WorkingDir $AppDir `
        -StdoutLog "$AppDir\logs\caddy.out.log" `
        -StderrLog "$AppDir\logs\caddy.err.log" `
        -Description "Caddy HTTPS reverse proxy for AI Stock Guardian"

    Write-Host "  Caddy: $CaddyExe"
    Write-Host "  Domain: https://$Domain"
} else {
    Write-Warn "No -Domain supplied, so Caddy HTTPS setup was skipped. For live trading, use a domain and HTTPS."
}
Write-Ok

Write-Host "[8/8] Locking down Windows Firewall and MT5 autostart..."
Get-NetFirewallRule -DisplayName "Guardian API Inbound (8150)" -ErrorAction SilentlyContinue | Remove-NetFirewallRule
Get-NetFirewallRule -DisplayName "Guardian API Direct Block (8150)" -ErrorAction SilentlyContinue | Remove-NetFirewallRule

if ($UseHttps) {
    New-NetFirewallRule -DisplayName "Guardian API Direct Block (8150)" -Direction Inbound -LocalPort 8150 -Protocol TCP -Action Block | Out-Null
    if (-not (Get-NetFirewallRule -DisplayName "Guardian HTTPS Inbound (443)" -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -DisplayName "Guardian HTTPS Inbound (443)" -Direction Inbound -LocalPort 443 -Protocol TCP -Action Allow | Out-Null
    }
    if (-not (Get-NetFirewallRule -DisplayName "Guardian HTTP Inbound (80)" -ErrorAction SilentlyContinue)) {
        New-NetFirewallRule -DisplayName "Guardian HTTP Inbound (80)" -Direction Inbound -LocalPort 80 -Protocol TCP -Action Allow | Out-Null
    }
} else {
    Write-Warn "No HTTPS domain supplied; opening direct inbound 8150 for temporary IP-only testing."
    New-NetFirewallRule -DisplayName "Guardian API Inbound (8150)" -Direction Inbound -LocalPort 8150 -Protocol TCP -Action Allow | Out-Null
}

$Mt5Terminal = Resolve-Mt5Terminal
if (-not [string]::IsNullOrWhiteSpace($Mt5ConfigPath) -and (Test-Path $Mt5ConfigPath) -and -not [string]::IsNullOrWhiteSpace($Mt5Terminal)) {
    $TaskName = "MetaTrader5AutoStart"
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
    $action = New-ScheduledTaskAction -Execute $Mt5Terminal -Argument "/config:`"$Mt5ConfigPath`""
    $trigger = New-ScheduledTaskTrigger -AtLogOn
    $principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Highest
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([timeSpan]::Zero)
    Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description "Auto-start MetaTrader 5 with local startup.ini" | Out-Null
    Start-ScheduledTask -TaskName $TaskName
    Write-Host "  MT5 autostart task installed."
} else {
    Write-Warn "MT5 autostart not installed. Pass -Mt5ConfigPath after creating startup.ini on the VPS."
}
Write-Ok

try {
    $PublicIP = (Invoke-WebRequest -uri "https://ifconfig.me/ip" -UseBasicParsing -TimeoutSec 5).Content.Trim()
} catch {
    $PublicIP = "<public-ip>"
}

$PublicBaseUrl = if ($UseHttps) { "https://$Domain" } else { "http://$PublicIP:8150" }

Write-Host ""
Write-Host "====================================================" -ForegroundColor Green
Write-Host "  Deployment Complete" -ForegroundColor Green
Write-Host "====================================================" -ForegroundColor Green
Write-Host "  Backend local health: http://127.0.0.1:8150/health"
Write-Host "  Public health:        $PublicBaseUrl/health" -ForegroundColor Cyan
Write-Host "  App API Base URL:     $PublicBaseUrl" -ForegroundColor Cyan
Write-Host "  Config:               $EnvFile"
Write-Host "  API logs:             $AppDir\logs\guardian-api.out.log"
Write-Host "  API errors:           $AppDir\logs\guardian-api.err.log"
if ($UseHttps) {
    Write-Host "  Caddy logs:           $AppDir\logs\caddy.out.log"
    Write-Host "  Caddy errors:         $AppDir\logs\caddy.err.log"
}
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Confirm MT5 is logged in on the VPS and Algo Trading is enabled."
Write-Host "  2. Test: Invoke-WebRequest $PublicBaseUrl/mt5/direct/status"
Write-Host "  3. In the C++ app, set API Base URL to: $PublicBaseUrl"
Write-Host "  4. Keep broker credentials only in MT5 or a VPS-local startup.ini."
Write-Host ""
