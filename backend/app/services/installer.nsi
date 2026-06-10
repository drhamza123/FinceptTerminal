; FinceptTerminal NSIS Installer
Unicode True
RequestExecutionLevel admin

!define PRODUCT_NAME "AI Stock Guardian"
!define PRODUCT_VERSION "4.1.0"
!define PRODUCT_PUBLISHER "Fincept Corporation"
!define PRODUCT_WEB_SITE "https://fincept.in"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "/tmp/nsis_build/AI_Stock_Guardian_Setup.exe"
InstallDir "$LOCALAPPDATA\AIStockGuardian"
InstallDirRegKey HKCU "Software\AIStockGuardian" ""

; Request application privileges for Windows Vista+
RequestExecutionLevel user

;------ Modern UI ------
!include "MUI2.nsh"
!include "FileFunc.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "fincept_icon.ico"
!define MUI_UNICON "fincept_icon.ico"
!define MUI_WELCOMEPAGE_TITLE "AI Stock Guardian v${PRODUCT_VERSION}"
!define MUI_WELCOMEPAGE_TEXT "This wizard will install AI Stock Guardian on your computer.$\r$\n$\r$\nAI-powered financial intelligence terminal with real-time trading, market analytics, and multi-broker support."
!define MUI_FINISHPAGE_RUN "$INSTDIR\FinceptTerminal.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch AI Stock Guardian"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

Section "Install" SecMain
    SetOutPath "$INSTDIR"
    
    ; Main executable
    File "FinceptTerminal.exe"
    
    ; MT5 worker & local server
    File "mt5_worker.exe"
    File "local_server.exe"
    
    ; All DLLs
    File /r "*.dll"
    
    ; Plugin directories
    File /r "platforms"
    File /r "styles"
    File /r "imageformats"
    File /r "sqldrivers"
    File /r "tls"
    File /r "iconengines"
    File /r "multimedia"
    File /r "networkinformation"
    File /r "generic"
    File /r "translations"
    
    ; Write uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    ; Registry
    WriteRegStr HKCU "Software\AIStockGuardian" "" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\AIStockGuardian" \
        "DisplayName" "AI Stock Guardian"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\AIStockGuardian" \
        "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\AIStockGuardian" \
        "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\AIStockGuardian" \
        "Publisher" "${PRODUCT_PUBLISHER}"
    
    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\AI Stock Guardian"
    CreateShortCut "$DESKTOP\AI Stock Guardian.lnk" "$INSTDIR\FinceptTerminal.exe" "" "$INSTDIR\FinceptTerminal.exe" 0
    CreateShortCut "$SMPROGRAMS\AI Stock Guardian\AI Stock Guardian.lnk" "$INSTDIR\FinceptTerminal.exe"
    CreateShortCut "$SMPROGRAMS\AI Stock Guardian\Uninstall.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    ; Remove files
    RMDir /r "$INSTDIR"
    
    ; Remove shortcuts
    Delete "$DESKTOP\AI Stock Guardian.lnk"
    RMDir /r "$SMPROGRAMS\AI Stock Guardian"
    
    ; Remove registry
    DeleteRegKey HKCU "Software\AIStockGuardian"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\AIStockGuardian"
SectionEnd
