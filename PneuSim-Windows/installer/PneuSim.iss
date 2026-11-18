; PneuSim installer (Inno Setup 6).
; Rebuilt on 18 Nov 2026 from the settings stored inside PneuSim-Setup-3.2.0.exe:
; same AppId, per-user install, shortcuts, file names and .pneu association, so a new build
; upgrades an existing 3.2.0 installation in place.
;
; Build:  ISCC.exe /DAppVersion=3.2.0 /DBinDir=<folder with PneuSim.exe> installer\PneuSim.iss

#ifndef AppVersion
  #define AppVersion "3.2.0"
#endif
#ifndef BinDir
  #define BinDir "..\src\PneuSim\bin\Release\net472"
#endif
#define Ex "..\examples\bundled"

[Setup]
AppId={{6F1E9A52-3C1B-4B7E-9D0A-5A2C3E7B8D41}
AppName=PneuSim
AppVersion={#AppVersion}
AppVerName=PneuSim {#AppVersion}
AppPublisher=Akshaya Simha
AppCopyright=(c) 2026 Akshaya Simha
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\PneuSim
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\PneuSim.exe
SetupIconFile=..\src\PneuSim\app.ico
OutputDir=output
OutputBaseFilename=PneuSim-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
english.AssocTask=Open .pneu circuit files with PneuSim

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "associate"; Description: "{cm:AssocTask}"; GroupDescription: "File types:"

[Files]
Source: "{#BinDir}\PneuSim.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BinDir}\PneuSim.exe.config"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\original\README-3.2.0-bundled.md"; DestDir: "{app}"; DestName: "README.md"; Flags: ignoreversion
Source: "{#Ex}\01_direct_single_acting.pneu"; DestDir: "{app}\Examples"; DestName: "ex1.pneu"; Flags: ignoreversion
Source: "{#Ex}\02_indirect_double_acting_speed_control.pneu"; DestDir: "{app}\Examples"; DestName: "ex2.pneu"; Flags: ignoreversion
Source: "{#Ex}\03_or_shuttle_valve.pneu"; DestDir: "{app}\Examples"; DestName: "ex3.pneu"; Flags: ignoreversion
Source: "{#Ex}\04_and_two_hand_control.pneu"; DestDir: "{app}\Examples"; DestName: "ex4.pneu"; Flags: ignoreversion
Source: "{#Ex}\05_auto_reciprocation_limit_valves.pneu"; DestDir: "{app}\Examples"; DestName: "ex5.pneu"; Flags: ignoreversion
Source: "{#Ex}\06_delayed_return_time_delay.pneu"; DestDir: "{app}\Examples"; DestName: "ex6.pneu"; Flags: ignoreversion
Source: "{#Ex}\07_electro_direct_solenoid.pneu"; DestDir: "{app}\Examples"; DestName: "ex7.pneu"; Flags: ignoreversion
Source: "{#Ex}\08_electro_self_holding.pneu"; DestDir: "{app}\Examples"; DestName: "ex8.pneu"; Flags: ignoreversion
Source: "{#Ex}\09_hydraulic_43_valve_relief.pneu"; DestDir: "{app}\Examples"; DestName: "ex9.pneu"; Flags: ignoreversion
Source: "{#Ex}\10_realistic_pressure_sequence.pneu"; DestDir: "{app}\Examples"; DestName: "ex10.pneu"; Flags: ignoreversion
Source: "{#Ex}\11_vacuum_handling.pneu"; DestDir: "{app}\Examples"; DestName: "ex11.pneu"; Flags: ignoreversion
Source: "{#Ex}\12_air_generation_receiver.pneu"; DestDir: "{app}\Examples"; DestName: "ex12.pneu"; Flags: ignoreversion
Source: "{#Ex}\gen_cascade_A+B+A-B-.pneu"; DestDir: "{app}\Generated examples"; DestName: "cascade_ApBpAmBm.pneu"; Flags: ignoreversion
Source: "{#Ex}\gen_cascade_A+B+B-A-.pneu"; DestDir: "{app}\Generated examples"; DestName: "cascade_ApBpBmAm.pneu"; Flags: ignoreversion
Source: "{#Ex}\gen_cascade_A+B+C+C-B-A-.pneu"; DestDir: "{app}\Generated examples"; DestName: "cascade_ApBpCpCmBmAm.pneu"; Flags: ignoreversion
Source: "{#Ex}\gen_relay_A+B+A-B-.pneu"; DestDir: "{app}\Generated examples"; DestName: "electro_ApBpAmBm.pneu"; Flags: ignoreversion
Source: "{#Ex}\gen_relay_A+B+B-A-.pneu"; DestDir: "{app}\Generated examples"; DestName: "electro_ApBpBmAm.pneu"; Flags: ignoreversion
Source: "{#Ex}\gen_relay_A+B+C+C-B-A-.pneu"; DestDir: "{app}\Generated examples"; DestName: "electro_ApBpCpCmBmAm.pneu"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\PneuSim"; Filename: "{app}\PneuSim.exe"
Name: "{autoprograms}\PneuSim examples"; Filename: "{app}\Examples"
Name: "{autodesktop}\PneuSim"; Filename: "{app}\PneuSim.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.pneu"; ValueType: string; ValueName: ""; ValueData: "PneuSim.Circuit"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PneuSim.Circuit"; ValueType: string; ValueName: ""; ValueData: "PneuSim circuit"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PneuSim.Circuit\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\PneuSim.exe,0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PneuSim.Circuit\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\PneuSim.exe"" ""%1"""; Tasks: associate

[Run]
Filename: "{app}\PneuSim.exe"; Description: "{cm:LaunchProgram,PneuSim}"; Flags: nowait postinstall skipifsilent
