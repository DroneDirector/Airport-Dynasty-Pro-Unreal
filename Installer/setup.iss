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
; where it goes: always over the copy already installed, wherever it is (owner, 2026-11-14), found from what the game
; and the installer remember (InstallDirGuess below); a first install goes to Program Files
DefaultDirName={code:InstallDirGuess}
UsePreviousAppDir=no
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
; GitHub takes files under 2 GiB: the setup in pieces (Setup.exe + Setup-1.bin, -2.bin ... beside it, 1.9 GB each)
DiskSpanning=yes
DiskSliceSize=1900000000
SlicesPerDisk=1
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "Make a shortcut on the desktop"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#MySrc}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; the shortcuts made fresh on every install: the old ones out first, for you and for all users (whichever way the last
; install was made), so they always start this copy
Type: files; Name: "{userdesktop}\Airport Dynasty Pro (Unreal).lnk"
Type: files; Name: "{commondesktop}\Airport Dynasty Pro (Unreal).lnk"
Type: files; Name: "{userprograms}\Airport Dynasty Pro (Unreal).lnk"
Type: files; Name: "{commonprograms}\Airport Dynasty Pro (Unreal).lnk"

[Registry]
; where this copy is (the game writes it too, each start)
Root: HKCU; Subkey: "Software\DroneDirector\AirportDynastyPro"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: uninsdeletevalue

[Icons]
Name: "{autoprograms}\Airport Dynasty Pro (Unreal)"; Filename: "{app}\AirportDynastyPro.exe"
Name: "{autodesktop}\Airport Dynasty Pro (Unreal)"; Filename: "{app}\AirportDynastyPro.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AirportDynastyPro.exe"; Description: "Start Airport Dynasty Pro"; Flags: nowait postinstall skipifsilent

[Code]
// The copy already installed: the folder the game itself remembered, else the one in Windows' list of installed
// programs (for all users or for you), else Program Files. Only a folder that really holds the game counts.
function HasGame(const Dir: String): Boolean;
begin
  Result := (Dir <> '') and FileExists(AddBackslash(Dir) + 'AirportDynastyPro.exe');
end;

function InstallDirGuess(Param: String): String;
var
  S, Key: String;
begin
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{3B7D9E21-6A4C-4F8E-9D15-2C7A0E4B8F63}_is1';
  if RegQueryStringValue(HKCU, 'Software\DroneDirector\AirportDynastyPro', 'InstallDir', S) and HasGame(S) then begin Result := RemoveBackslashUnlessRoot(S); exit; end;
  if RegQueryStringValue(HKLM64, Key, 'InstallLocation', S) and HasGame(S) then begin Result := RemoveBackslashUnlessRoot(S); exit; end;
  if RegQueryStringValue(HKLM32, Key, 'InstallLocation', S) and HasGame(S) then begin Result := RemoveBackslashUnlessRoot(S); exit; end;
  if RegQueryStringValue(HKCU, Key, 'InstallLocation', S) and HasGame(S) then begin Result := RemoveBackslashUnlessRoot(S); exit; end;
  Result := ExpandConstant('{autopf}\Airport Dynasty Pro Unreal');
end;
