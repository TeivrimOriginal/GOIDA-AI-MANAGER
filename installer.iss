#define MyAppName "GOIDA AI MANAGER"
#define MyAppVersion "2.4"
#define MyAppPublisher "TeivrimOriginal"
#define MyAppExeName "GOIDA.exe"

[Setup]
AppId={{B5C9B0FD-5C4F-4B38-AE3D-08B0BCE8A6C1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\GOIDA AI MANAGER
DefaultGroupName=GOIDA AI MANAGER
DisableProgramGroupPage=yes
OutputDir=release
OutputBaseFilename=GOIDA-AI-MANAGER-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
Source: "build\GOIDA.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "lang\ru.json"; DestDir: "{app}\lang"; Flags: ignoreversion
Source: "lang\en.json"; DestDir: "{app}\lang"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "TODO.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\GOIDA"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall GOIDA"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Запустить GOIDA AI MANAGER"; Flags: nowait postinstall skipifsilent
