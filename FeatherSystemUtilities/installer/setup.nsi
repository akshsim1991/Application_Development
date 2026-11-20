; Feather System Utilities - offline installer (NSIS, zlib license). Build: ./build.sh installer
Unicode true
!define APP "Feather System Utilities"
!define VER "3.2.0"
!define EXE "FeatherSystemUtilities.exe"
!define UNKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\FeatherSystemUtilities"
!include "MUI2.nsh"
!include "x64.nsh"

Name "${APP}"
OutFile "..\build\FeatherSystemUtilities-Setup-${VER}.exe"
InstallDir "$PROGRAMFILES64\${APP}"
InstallDirRegKey HKLM "${UNKEY}" "InstallLocation"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "Developed by Akshaya Simha"
VIProductVersion "3.2.0.0"
VIAddVersionKey "ProductName" "${APP}"
VIAddVersionKey "FileDescription" "${APP} Setup"
VIAddVersionKey "FileVersion" "${VER}"
VIAddVersionKey "LegalCopyright" "Developed by Akshaya Simha"

!define MUI_ICON "..\src\win\app.ico"
!define MUI_UNICON "..\src\win\app.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Open ${APP}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
FunctionEnd

Section "Install"
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  ; close a running copy gracefully (upgrade)
  FindWindow $0 "FeatherSystemUtilitiesWnd"
  ${If} $0 != 0
    SendMessage $0 0x0010 0 0
    Sleep 1500
  ${EndIf}
  SetOutPath "$INSTDIR"
  ${If} ${RunningX64}
    File "..\build\${EXE}"
    File "..\build\WebView2Loader.dll"
  ${Else}
    File /oname=${EXE} "..\build\FeatherSystemUtilities-x86.exe"
    File /oname=WebView2Loader.dll "..\build\WebView2Loader-x86.dll"
  ${EndIf}
  File "..\README.md"
  File "..\THIRD_PARTY_NOTICES.md"
  File "..\docs\USER_GUIDE.md"
  CreateDirectory "$SMPROGRAMS\${APP}"
  CreateShortcut "$SMPROGRAMS\${APP}\${APP}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${UNKEY}" "DisplayName" "${APP}"
  WriteRegStr HKLM "${UNKEY}" "DisplayVersion" "${VER}"
  WriteRegStr HKLM "${UNKEY}" "Publisher" "Akshaya Simha"
  WriteRegStr HKLM "${UNKEY}" "DisplayIcon" "$INSTDIR\${EXE}"
  WriteRegStr HKLM "${UNKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNKEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ${If} ${RunningX64}
    SetRegView 64
  ${EndIf}
  FindWindow $0 "FeatherSystemUtilitiesWnd"
  ${If} $0 != 0
    SendMessage $0 0x0010 0 0
    Sleep 1500
  ${EndIf}
  ; hand back any startup apps delayed by this program, then remove its scheduled tasks
  ExecWait '"$INSTDIR\${EXE}" --cleanup'
  Delete "$INSTDIR\${EXE}"
  Delete "$INSTDIR\WebView2Loader.dll"
  RMDir /r "$LOCALAPPDATA\FeatherSystemUtilities\webview2"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.md"
  Delete "$INSTDIR\USER_GUIDE.md"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${APP}\${APP}.lnk"
  RMDir "$SMPROGRAMS\${APP}"
  DeleteRegKey HKLM "${UNKEY}"
  ; user data (settings, journal, backups, profiles) is kept unless the user opts to remove it
  IfSilent done
  MessageBox MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2 "Also delete your settings, change history, deleted-entry backups and profiles?$\r$\n$\r$\n(Choose No to keep them so you can still restore deleted startup entries.)" IDNO done
  RMDir /r "$LOCALAPPDATA\FeatherSystemUtilities"
  done:
SectionEnd
