; Feather Clipboard Manager - installer (NSIS). Per-user: no administrator permission is needed.
; Build:  makensis -DVERSION=2.2.0 -DEXE=build\FeatherClipboardManager.exe -DOUT=releases\FeatherClipboardManager-Setup-2.2.0.exe installer\FeatherClipboardManager.nsi
; Silent (used by the in-app updater):  Setup.exe /S /relaunch /waitfor=<process id>
Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

!ifndef VERSION
  !define VERSION "2.2.0"
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
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header.bmp"

; One screen like Feather PDF: the install location and a single Install button, then progress, then the program starts.
Page custom MainPageCreate MainPageLeave
!define MUI_PAGE_HEADER_TEXT "Installing ${APPNAME}"
!define MUI_PAGE_HEADER_SUBTEXT "Version ${VERSION}"
!insertmacro MUI_PAGE_INSTFILES
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

Var Dlg
Var PathEdit
Var ActionWord

Function MainPageCreate
  ; is it already installed? then this is an update (or a repair) in the same place
  StrCpy $ActionWord "Install"
  ReadRegStr $R0 HKCU "${UNINSTKEY}" "DisplayVersion"
  ReadRegStr $R1 HKCU "${UNINSTKEY}" "InstallLocation"
  ${If} $R1 != ""
    StrCpy $INSTDIR $R1
    ${If} $R0 == "${VERSION}"
      StrCpy $ActionWord "Repair"
    ${Else}
      StrCpy $ActionWord "Update"
    ${EndIf}
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "Setup" "Version ${VERSION}  -  clipboard history that stays on your PC"
  nsDialogs::Create 1018
  Pop $Dlg
  ${If} $Dlg == error
    Abort
  ${EndIf}

  CreateFont $R5 "$(^Font)" 13 700
  ${NSD_CreateLabel} 0 4u 100% 16u "${APPNAME}"
  Pop $0
  SendMessage $0 ${WM_SETFONT} $R5 0
  ${NSD_CreateLabel} 0 22u 100% 24u "Keeps what you copy - text, pictures and files - on this PC only, so you can search it and paste it again. No account, no cloud."
  Pop $0

  ${NSD_CreateLabel} 0 56u 100% 10u "Install location"
  Pop $0
  ${NSD_CreateText} 0 68u 80% 12u "$INSTDIR"
  Pop $PathEdit
  SendMessage $PathEdit ${EM_SETREADONLY} 1 0
  ${NSD_CreateButton} 83% 66u 17% 14u "Change..."
  Pop $0
  ${NSD_OnClick} $0 OnChangeDir

  ${If} $ActionWord == "Update"
    ${NSD_CreateLabel} 0 90u 100% 24u "Version $R0 is installed. It will be updated to ${VERSION} in the same place. Your history and settings stay."
    Pop $0
  ${ElseIf} $ActionWord == "Repair"
    ${NSD_CreateLabel} 0 90u 100% 24u "This version is already installed. Installing it again repairs it. Your history and settings stay."
    Pop $0
  ${Else}
    ${NSD_CreateLabel} 0 90u 100% 24u "It needs about 2 MB and no administrator permission. Your history will be kept in %LOCALAPPDATA%\FeatherClipboardManager."
    Pop $0
  ${EndIf}

  ; one button: Install / Update / Repair
  GetDlgItem $0 $HWNDPARENT 1
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$ActionWord"
  GetDlgItem $0 $HWNDPARENT 3
  ShowWindow $0 ${SW_HIDE}
  nsDialogs::Show
FunctionEnd

Function OnChangeDir
  nsDialogs::SelectFolderDialog "Choose the folder to install ${APPNAME} in" "$INSTDIR"
  Pop $0
  ${If} $0 != error
    ${NSD_SetText} $PathEdit "$0"
  ${EndIf}
FunctionEnd

Function MainPageLeave
  ${NSD_GetText} $PathEdit $INSTDIR
  ${If} $INSTDIR == ""
    MessageBox MB_OK|MB_ICONEXCLAMATION "Choose a folder to install in."
    Abort
  ${EndIf}
FunctionEnd

Section "Install"
  SetAutoClose true
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

  ; the program starts when the installation is done (silently only when the updater asked for it)
  ${If} $Relaunch == "1"
    Exec '"$INSTDIR\${EXENAME}"'
  ${ElseIfNot} ${Silent}
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
