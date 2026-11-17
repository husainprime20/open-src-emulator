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
; Self-contained install on the system (C:) drive:
;   C:\OpenSrc            application + Qt runtime (this installer)
;   C:\OpenSrc\Runtime    Android runtime OpenSrc downloads on first setup
;                         (Java, SDK tools, emulator, system image, virtual device)
DefaultDirName=C:\OpenSrc
DisableDirPage=no
UsePreviousAppDir=yes
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir=..\artifacts
OutputBaseFilename=OpenSrc-Setup-{#AppVersion}-x64
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; "lowest" lets a standard user install into their own C:\OpenSrc folder without
; a UAC prompt; setup elevates only if the user picks a protected location.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
LicenseFile=..\LICENSE
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#AppName} — Android emulator
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Dirs]
; Grant the invoking user write access to the runtime subtree so OpenSrc can
; download several gigabytes of Android components later without elevation.
; Only this app-private data folder is modified; program files keep default ACLs.
Name: "{app}\Runtime"; Permissions: users-modify
Name: "{app}\Runtime\sdk"; Permissions: users-modify
Name: "{app}\Runtime\avd"; Permissions: users-modify
Name: "{app}\Runtime\android-user"; Permissions: users-modify

[Files]
Source: "..\package\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD-PARTY-NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\OpenSrc"; Filename: "{app}\{#AppExeName}"
Name: "{group}\OpenSrc runtime folder"; Filename: "{app}\Runtime"
Name: "{autodesktop}\OpenSrc"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Parameters: "--setup-runtime"; Description: "Launch OpenSrc and set up Android"; Flags: postinstall nowait skipifsilent

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  RuntimeDir: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    RuntimeDir := ExpandConstant('{app}\Runtime');
    if DirExists(RuntimeDir) then
    begin
      if MsgBox('Also delete the downloaded Android runtime and virtual device data?' + #13#10 +
                #13#10 + RuntimeDir + #13#10 + #13#10 +
                'Choose No to keep your Android apps, data, and downloaded system image.',
                mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(RuntimeDir, True, True, True);
    end;
  end;
end;
