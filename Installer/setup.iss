; The PC installer for the Unreal edition (Inno Setup, free for commercial use). Its own app id, folder, shortcuts and
; uninstall entry, so it sits beside the Babylon edition's PC app until it replaces it. A newer version installs over
; the old one and keeps your settings and saves (they live in the user's own folder, not here).
#define MyVer GetEnv('ADP_VERSION')
#define MySrc GetEnv('ADP_BUILD')
[Setup]
AppId={{3B7D9E21-6A4C-4F8E-9D15-2C7A0E4B8F63}
AppName=Airport Dynasty Pro (Unreal)
AppVersion={#MyVer}
AppVerName=Airport Dynasty Pro (Unreal) {#MyVer}
AppPublisher=DroneDirector
DefaultDirName={autopf}\Airport Dynasty Pro Unreal
DefaultGroupName=Airport Dynasty Pro
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\Packaged
OutputBaseFilename=AirportDynastyPro-Unreal-PC-Setup-{#MyVer}
SetupIconFile=app.ico
UninstallDisplayIcon={app}\AirportDynastyPro.exe
UninstallDisplayName=Airport Dynasty Pro (Unreal)
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "Make a shortcut on the desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#MySrc}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Airport Dynasty Pro (Unreal)"; Filename: "{app}\AirportDynastyPro.exe"
Name: "{autodesktop}\Airport Dynasty Pro (Unreal)"; Filename: "{app}\AirportDynastyPro.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AirportDynastyPro.exe"; Description: "Start Airport Dynasty Pro"; Flags: nowait postinstall skipifsilent
