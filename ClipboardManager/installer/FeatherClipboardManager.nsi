; Feather Clipboard Manager - installer (NSIS). Per-user: no administrator permission is needed.
; Build:  makensis -DVERSION=2.1.0 -DEXE=build\FeatherClipboardManager.exe -DOUT=releases\FeatherClipboardManager-Setup-2.1.0.exe installer\FeatherClipboardManager.nsi
; Silent (used by the in-app updater):  Setup.exe /S /relaunch /waitfor=<process id>
Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

!ifndef VERSION
  !define VERSION "2.1.0"
!endif
!ifndef EXE
  !define EXE "..\build\FeatherClipboardManager.exe"
!endif
!ifndef OUT
  !define OUT "..\releases\FeatherClipboardManager-Setup-${VERSION}.exe"
!endif
!ifndef ICON
  !define ICON "..\src\res\clipboard.ico"
!endif

!define APPNAME "Feather Clipboard Manager"
!define EXENAME "FeatherClipboardManager.exe"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\FeatherClipboardManager"

Name "${APPNAME}"
OutFile "${OUT}"
InstallDir "$LOCALAPPDATA\Programs\Feather Clipboard Manager"
InstallDirRegKey HKCU "Software\FeatherClipboardManager" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "${APPNAME} ${VERSION}"
ShowInstDetails nevershow
ShowUninstDetails nevershow

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "CompanyName" "Akshaya Simha"
VIAddVersionKey "FileDescription" "${APPNAME} setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "(c) 2026 Akshaya Simha"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "${APPNAME} ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "This installs ${APPNAME} for your Windows account. It keeps what you copy - text, pictures and files - on this PC only, so you can search and paste it again.$\r$\n$\r$\nNo administrator permission is needed. Your history is kept in %LOCALAPPDATA%\FeatherClipboardManager and is not touched when you update or uninstall.$\r$\n$\r$\nClick Install to continue."
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXENAME}"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${APPNAME} now"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Start ${APPNAME} automatically when I sign in to Windows"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION SetAutostartFromFinish
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Var WaitPid
Var Relaunch

; Ask a running copy to quit (it ends cleanly and saves), then make sure it is gone.
!macro StopRunning
  IfFileExists "$INSTDIR\${EXENAME}" 0 +3
    ExecWait '"$INSTDIR\${EXENAME}" --exit'
    Sleep 1200
  nsExec::Exec 'taskkill /F /IM ${EXENAME}'
  Pop $0
  Sleep 300
!macroend

Function .onInit
  ${GetParameters} $R0
  ${GetOptions} $R0 "/waitfor=" $WaitPid
  ClearErrors
  ${GetOptions} $R0 "/relaunch" $R1
  ${If} ${Errors}
    StrCpy $Relaunch "0"
  ${Else}
    StrCpy $Relaunch "1"
  ${EndIf}
  ClearErrors
  ; the updater starts this installer from the program itself: wait until that process has ended
  ${If} $WaitPid != ""
    System::Call 'kernel32::OpenProcess(i 0x100000, i 0, i $WaitPid) p .r0'
    ${If} $0 != 0
      System::Call 'kernel32::WaitForSingleObject(p r0, i 20000)'
      System::Call 'kernel32::CloseHandle(p r0)'
    ${EndIf}
  ${EndIf}
FunctionEnd

Function SetAutostartFromFinish
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FeatherClipboardManager" '"$INSTDIR\${EXENAME}" /tray'
FunctionEnd

Section "Install"
  !insertmacro StopRunning
  SetOutPath "$INSTDIR"
  File "${EXE}"
  File "..\docs\USER_GUIDE.md"
  File "..\README.md"
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${EXENAME}" "" "$INSTDIR\${EXENAME}" 0
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "Software\FeatherClipboardManager" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "Akshaya Simha"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\${EXENAME}"
  WriteRegStr HKCU "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINSTKEY}" "EstimatedSize" "$0"

  ; an existing start-with-Windows entry keeps pointing at the right place after an update or a move
  ReadRegStr $3 HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FeatherClipboardManager"
  ${If} $3 != ""
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FeatherClipboardManager" '"$INSTDIR\${EXENAME}" /tray'
  ${EndIf}

  ${If} $Relaunch == "1"
    Exec '"$INSTDIR\${EXENAME}"'
  ${EndIf}
SectionEnd

Section "Uninstall"
  IfFileExists "$INSTDIR\${EXENAME}" 0 +3
    ExecWait '"$INSTDIR\${EXENAME}" --exit'
    Sleep 1200
  nsExec::Exec 'taskkill /F /IM ${EXENAME}'
  Pop $0
  Delete "$INSTDIR\${EXENAME}"
  Delete "$INSTDIR\USER_GUIDE.md"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "FeatherClipboardManager"
  DeleteRegKey HKCU "${UNINSTKEY}"

  ; the history and settings are yours: they are only removed when you say so
  ${GetParameters} $R0
  ${GetOptions} $R0 "/DELETEDATA" $R1
  ${IfNot} ${Errors}
    Goto deleteData
  ${EndIf}
  ClearErrors
  IfSilent keepData
  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "Also delete your clipboard history and settings?$\r$\n$\r$\n$LOCALAPPDATA\FeatherClipboardManager$\r$\n$\r$\nChoose No to keep them (for example to reinstall later)." IDYES deleteData IDNO keepData
  deleteData:
    RMDir /r "$LOCALAPPDATA\FeatherClipboardManager"
    DeleteRegKey HKCU "Software\FeatherClipboardManager"
  keepData:
  DeleteRegKey HKCU "Software\FeatherClipboardManager\InstallDirOnly"
SectionEnd
