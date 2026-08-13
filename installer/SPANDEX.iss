; Inno Setup script for SPANDEX.
; Lets the user pick the Standalone app, the VST3 plugin, or both.
; Compile with: ISCC.exe installer\SPANDEX.iss

#define MyAppName "SPANDEX"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "SPANDEX"
#define ReleaseDir "..\build\SPANDEX_artefacts\Release"

[Setup]
AppId={{8F2B6C9E-4E31-4B7B-9A6D-6F0C6D6E7A11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=SPANDEX-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "standalone"; Description: "Standalone Application (play files directly)"
Name: "vst3"; Description: "VST3 Plugin (use inside a DAW)"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Components: standalone

[Files]
Source: "{#ReleaseDir}\Standalone\SPANDEX.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "..\dist\README.txt"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "{#ReleaseDir}\VST3\SPANDEX.vst3\*"; DestDir: "{commoncf}\VST3\SPANDEX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\SPANDEX.exe"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\SPANDEX.exe"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\SPANDEX.exe"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent; Components: standalone

[Messages]
WelcomeLabel2=This will install {#MyAppName} on your computer.%n%nChoose the Standalone application, the VST3 plugin for use inside a DAW, or both on the next page.
