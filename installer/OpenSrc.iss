#ifndef AppVersion
  #define AppVersion "0.2.0"
#endif

#define AppName "OpenSrc"
#define AppPublisher "OpenSrc contributors"
#define AppExeName "OpenSrc.exe"

[Setup]
AppId={{75AB10C7-C477-444C-A477-CE7E1E6A7C58}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://github.com/husainprime20/open-src-emulator
DefaultDirName={autopf}\OpenSrc
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir=..\artifacts
OutputBaseFilename=OpenSrc-Setup-{#AppVersion}-x64
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
LicenseFile=..\LICENSE
WizardStyle=modern
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "..\package\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\OpenSrc"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\OpenSrc"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch OpenSrc"; Flags: postinstall nowait skipifsilent
