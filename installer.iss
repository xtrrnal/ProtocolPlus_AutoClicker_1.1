#define MyAppName "Protocol+ Auto Clicker"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "Protocol+"
#define MyAppExeName "ProtocolPlus_Auto_Clicker.exe"
[Setup]
AppId={{D4F1A9A8-7E3D-4E7E-8F1E-2B3E9E2C7A11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Protocol+\Auto Clicker
DefaultGroupName=Protocol+ Auto Clicker
OutputDir=release
OutputBaseFilename=ProtocolPlus_Auto_Clicker_Setup
SetupIconFile=resources\protocolplus.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
UninstallDisplayName={#MyAppName}
ArchitecturesInstallIn64BitMode=x64compatible
[Files]
Source: "build\ProtocolPlus_Auto_Clicker.exe"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{autoprograms}\Protocol+ Auto Clicker"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\Protocol+ Auto Clicker"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"
[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Protocol+ Auto Clicker"; Flags: nowait postinstall skipifsilent
