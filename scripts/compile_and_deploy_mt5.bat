@echo off
REM ============================================================
REM Compile & Deploy GuardianBridge EA to MetaTrader 5
REM Run this on a Windows machine with MT5 installed
REM ============================================================
setlocal enabledelayedexpansion

REM --- Config ---
set EA_NAME=GuardianBridge
set MQL5_SRC=%~dp0..\backend\mql5\%EA_NAME%\%EA_NAME%.mq5
if not exist "%MQL5_SRC%" (
    echo ERROR: Source file not found at %MQL5_SRC%
    exit /b 1
)

REM --- Find metaeditor64.exe ---
set METAEDITOR=C:\Program Files\MetaTrader 5\metaeditor64.exe
if not exist "%METAEDITOR%" set METAEDITOR=C:\Program Files\MetaTrader 5\metaeditor.exe
if not exist "%METAEDITOR%" (
    echo Searching for MetaEditor...
    dir /s /b "%PROGRAMFILES%\metaeditor*.exe" 2>nul >%TEMP%\me_find.txt
    set /p METAEDITOR=<%TEMP%\me_find.txt
)
if not exist "!METAEDITOR!" (
    echo ERROR: metaeditor64.exe not found. Is MetaTrader 5 installed?
    exit /b 1
)
echo Using: !METAEDITOR!

REM --- Find Experts folder ---
for /f "tokens=2*" %%a in ('reg query "HKCU\Software\MetaQuotes\Terminal" /v "Common" 2^>nul') do set MT5_COMMON=%%b
if defined MT5_COMMON (
    set EXPERTS_DIR=!MT5_COMMON!\Experts
) else (
    set EXPERTS_DIR=%APPDATA%\MetaQuotes\Terminal\Common\Experts
)
echo Experts dir: !EXPERTS_DIR!

REM --- Create temp workspace ---
set TMP_DIR=%TEMP%\%EA_NAME%_build
if not exist "%TMP_DIR%" mkdir "%TMP_DIR%"
copy "%MQL5_SRC%" "%TMP_DIR%\%EA_NAME%.mq5" /Y

REM --- Compile ---
echo Compiling %EA_NAME%...
"!METAEDITOR!" /compile:"%TMP_DIR%\%EA_NAME%.mq5" /log:"%TMP_DIR%\compile.log"
echo --- Compile Log ---
type "%TMP_DIR%\compile.log"
echo --------------------

REM --- Deploy ---
if exist "%TMP_DIR%\%EA_NAME%.ex5" (
    if not exist "!EXPERTS_DIR!" mkdir "!EXPERTS_DIR!"
    copy "%TMP_DIR%\%EA_NAME%.ex5" "!EXPERTS_DIR!\%EA_NAME%.ex5" /Y
    echo SUCCESS: Deployed to !EXPERTS_DIR!\%EA_NAME%.ex5
    echo.
    echo Next steps:
    echo 1. Open MetaTrader 5
    echo 2. In Navigator, expand Expert Advisors
    echo 3. Drag GuardianBridge onto your chart
    echo 4. Ensure InpServerHost=127.0.0.1 and InpServerPort=5556
    echo 5. Make sure the backend is running on port 8150 with TCP on 5556
) else (
    echo ERROR: Compilation failed - check compile.log above
)

rmdir /s /q "%TMP_DIR%" 2>nul
endlocal
pause
