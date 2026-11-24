; Small installer: no .NET runtime inside (a couple of MB). Needs the free ".NET 10 Desktop Runtime" (x64) on the PC.
; Build: iscc /DAppVersion=2.1.0 installer\FeatherView-small.iss     (uses publish\fxdep from the framework-dependent publish)
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define AppName "Feather View"

[Setup]
AppId={{B7E0B4F2-5D2A-4B4C-9C61-FEA7E4A11A01}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Akshaya Simha
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputDir=..\dist
OutputBaseFilename=FeatherView-Setup-small-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\src\FeatherView.App\Assets\app.ico
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\FeatherView.exe
CloseApplications=yes
DisableProgramGroupPage=yes

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; Flags: unchecked
Name: "assoc"; Description: "Show Feather View in the ""Open with"" list for pictures"

[Files]
Source: "..\publish\fxdep\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\FeatherView.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\FeatherView.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\FeatherView.Image"; ValueType: string; ValueData: "Picture (Feather View)"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\FeatherView.Image\DefaultIcon"; ValueType: string; ValueData: "{app}\FeatherView.exe,0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\FeatherView.Image\shell\open\command"; ValueType: string; ValueData: """{app}\FeatherView.exe"" ""%1"""; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.jpg\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.jpeg\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.png\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.bmp\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.gif\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.tif\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.tiff\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.webp\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.ico\OpenWithProgids"; ValueType: string; ValueName: "FeatherView.Image"; ValueData: ""; Flags: uninsdeletevalue; Tasks: assoc

[Run]
; Register "Open with" for pictures AND videos (per user, no administrator rights needed)
Filename: "{app}\FeatherView.exe"; Parameters: "--register"; Flags: runhidden runasoriginaluser; Tasks: assoc
Filename: "{app}\FeatherView.exe"; Description: "Start {#AppName}"; Flags: nowait postinstall runasoriginaluser

[UninstallRun]
Filename: "{app}\FeatherView.exe"; Parameters: "--unregister"; Flags: runhidden; RunOnceId: "FeatherViewUnregister"

[Code]
function HasDotNetDesktop10(): Boolean;
var
  Names: TArrayOfString;
  I: Integer;
begin
  Result := False;
  if RegGetValueNames(HKLM64, 'SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedfx\Microsoft.WindowsDesktop.App', Names) then
    for I := 0 to GetArrayLength(Names) - 1 do
      if Pos('10.', Names[I]) = 1 then
      begin
        Result := True;
        Exit;
      end;
end;

function InitializeSetup(): Boolean;
var
  ErrorCode: Integer;
begin
  Result := HasDotNetDesktop10();
  if not Result and not WizardSilent() then
    if MsgBox('This small installer needs the free Microsoft ".NET 10 Desktop Runtime" (x64). It is a one-time download of about 55 MB.' + #13#10#13#10 +
              'Open the download page now?' + #13#10 + '(Or use the larger Feather View setup file, which includes everything.)', mbConfirmation, MB_YESNO) = IDYES then
      ShellExec('open', 'https://dotnet.microsoft.com/download/dotnet/10.0', '', '', SW_SHOWNORMAL, ewNoWait, ErrorCode);
end;
