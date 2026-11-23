; Installer for Feather Image and Video Viewer. Built with NSIS (plain EXE installer: no Windows Installer
; service, no .NET, so it also runs in Safe Mode). Picks the 64-bit or 32-bit program automatically.
Unicode true
!define APP "Feather Image and Video Viewer"
!define VER "0.3.0"
!define PUB "Akshaya Simha"
!define UNKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\FeatherViewer"

!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

Name "${APP}"
OutFile "dist\FeatherViewer-Setup-${VER}.exe"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "${APP} ${VER}"
VIProductVersion "${VER}.0"
VIAddVersionKey "ProductName" "${APP}"
VIAddVersionKey "FileDescription" "${APP} installer"
VIAddVersionKey "FileVersion" "${VER}"
VIAddVersionKey "CompanyName" "${PUB}"

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\FeatherViewer.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${If} ${RunningX64}
    SetRegView 64
    StrCpy $INSTDIR "$PROGRAMFILES64\${APP}"
  ${Else}
    StrCpy $INSTDIR "$PROGRAMFILES\${APP}"
  ${EndIf}
FunctionEnd

Section "${APP} (required)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  ${If} ${RunningX64}
    File "/oname=FeatherViewer.exe" "dist\FeatherViewer64.exe"
  ${Else}
    File "/oname=FeatherViewer.exe" "dist\FeatherViewer32.exe"
  ${EndIf}
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; "Open with" registration
  WriteRegStr HKLM "Software\Classes\FeatherViewer.Image" "" "Image (Feather Viewer)"
  WriteRegStr HKLM "Software\Classes\FeatherViewer.Image\DefaultIcon" "" "$INSTDIR\FeatherViewer.exe,0"
  WriteRegStr HKLM "Software\Classes\FeatherViewer.Image\shell\open\command" "" '"$INSTDIR\FeatherViewer.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\Applications\FeatherViewer.exe\shell\open\command" "" '"$INSTDIR\FeatherViewer.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\Applications\FeatherViewer.exe" "FriendlyAppName" "${APP}"

  ; Add/Remove Programs
  WriteRegStr HKLM "${UNKEY}" "DisplayName" "${APP}"
  WriteRegStr HKLM "${UNKEY}" "DisplayVersion" "${VER}"
  WriteRegStr HKLM "${UNKEY}" "Publisher" "${PUB}"
  WriteRegStr HKLM "${UNKEY}" "DisplayIcon" "$INSTDIR\FeatherViewer.exe"
  WriteRegStr HKLM "${UNKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKLM "${UNKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNKEY}" "NoRepair" 1
SectionEnd

Section "Start Menu shortcut" SecStart
  CreateDirectory "$SMPROGRAMS\${APP}"
  CreateShortcut "$SMPROGRAMS\${APP}\${APP}.lnk" "$INSTDIR\FeatherViewer.exe"
  CreateShortcut "$SMPROGRAMS\${APP}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Desktop shortcut" SecDesk
  CreateShortcut "$DESKTOP\${APP}.lnk" "$INSTDIR\FeatherViewer.exe"
SectionEnd

Section "Offer as an option in 'Open with' for images" SecAssoc
  !macro ASSOC ext
    WriteRegStr HKLM "Software\Classes\${ext}\OpenWithProgids" "FeatherViewer.Image" ""
  !macroend
  !insertmacro ASSOC ".jpg"
  !insertmacro ASSOC ".jpeg"
  !insertmacro ASSOC ".png"
  !insertmacro ASSOC ".bmp"
  !insertmacro ASSOC ".gif"
  !insertmacro ASSOC ".tif"
  !insertmacro ASSOC ".tiff"
  !insertmacro ASSOC ".ico"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\FeatherViewer.exe"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${APP}\${APP}.lnk"
  Delete "$SMPROGRAMS\${APP}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${APP}"
  Delete "$DESKTOP\${APP}.lnk"
  DeleteRegKey HKLM "Software\Classes\FeatherViewer.Image"
  DeleteRegKey HKLM "Software\Classes\Applications\FeatherViewer.exe"
  !macro UNASSOC ext
    DeleteRegValue HKLM "Software\Classes\${ext}\OpenWithProgids" "FeatherViewer.Image"
  !macroend
  !insertmacro UNASSOC ".jpg"
  !insertmacro UNASSOC ".jpeg"
  !insertmacro UNASSOC ".png"
  !insertmacro UNASSOC ".bmp"
  !insertmacro UNASSOC ".gif"
  !insertmacro UNASSOC ".tif"
  !insertmacro UNASSOC ".tiff"
  !insertmacro UNASSOC ".ico"
  DeleteRegKey HKLM "${UNKEY}"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd
