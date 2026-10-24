; Inno Setup script for Feather PDF. Build with: ISCC installer\FeatherPDF.iss  (after building FeatherPDF.exe)
#define AppName "Feather PDF"
#define AppVersion "2.13.0"
#define AppExe "FeatherPDF.exe"
#define BuildDir "..\build"

[Setup]
AppId={{B6F5A1E2-7D43-4C8E-9A5B-FEA7E2C0D913}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Akshaya Simha
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=FeatherPDF-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#AppExe}
MinVersion=10.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Installs for the current user without an administrator prompt; the user can choose "all users" at the start.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
SetupLogging=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked
Name: "openwith"; Description: "Show Feather PDF in the ""Open with"" list for PDF files"; GroupDescription: "File types:"

[Files]
Source: "{#BuildDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\pdfium.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs
Source: "{#BuildDir}\tessdata\*"; DestDir: "{app}\tessdata"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\FeatherPDF.Document"; ValueType: string; ValueName: ""; ValueData: "PDF document"; Flags: uninsdeletekey; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\FeatherPDF.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\FeatherPDF.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.pdf\OpenWithProgids"; ValueType: none; ValueName: "FeatherPDF.Document"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith
Root: HKA; Subkey: "Software\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "FeatherPDF.Document"; ValueData: ""; Flags: uninsdeletevalue; Tasks: openwith

[Run]
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
