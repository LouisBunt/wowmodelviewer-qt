; ModelViewer: Midnight - Inno Setup installer script
; -----------------------------------------------------------------------------
; Per-user installer, no admin required. The application writes its config, log and
; database cache next to its own executable, so it must live somewhere the user can
; write -- hence PrivilegesRequired=lowest, which resolves {autopf} to
; %LOCALAPPDATA%\Programs. The VC++ runtime ships app-local, so there is no redist
; step either.
;
; The payload is whatever installer\stage contains. Run stage.ps1 first -- it copies
; each file from the place that produces it, so the file list lives there rather than
; being duplicated and drifting here.
;
;   powershell -ExecutionPolicy Bypass -File installer\stage.ps1
;   "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" installer\MVMidnight.iss
;
; Output: installer\dist\MV-Midnight-Setup-<version>.exe

#define MyAppName "ModelViewer: Midnight"
#define MyAppVersion "1.7.0"
#define MyAppVersionNumeric "1.7.0.0"
#define MyAppPublisher "Skogdesign"
#define MyAppExeName "WoWModelViewer-Qt.exe"
#define MyAppURL "https://github.com/LouisBunt/wowmodelviewer-qt"
#define Stage "stage"
#define IconsDir "..\upstream\bin_support\Icons"

[Setup]
; Same AppId as WoW Model Viewer Midnight on purpose: this build replaces the
; wxWidgets one rather than installing beside it, so Inno finds the existing
; installation and upgrades it in place.
AppId={{B7E9F3A2-1C4D-4E8A-9F6B-2A3C5D7E9F10}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
VersionInfoVersion={#MyAppVersionNumeric}
VersionInfoProductVersion={#MyAppVersionNumeric}
DefaultDirName={autopf}\ModelViewer Midnight
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=dist
OutputBaseFilename=MV-Midnight-Setup-{#MyAppVersion}
SetupIconFile=..\resources\appicon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
; GPLv3 -- shown during setup and installed alongside the binary.
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "german";  MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; Upgrading over the wxWidgets installation: strip its executable and toolkit DLLs.
; Left in place they are dead weight, and a stale wowmodelviewer.exe next to the new
; build is an invitation to start the wrong one and report bugs against it.
Type: files; Name: "{app}\wowmodelviewer.exe"
Type: files; Name: "{app}\wxbase32u_*.dll"
Type: files; Name: "{app}\wxmsw32u_*.dll"
Type: files; Name: "{app}\jpeg62.dll"
Type: files; Name: "{app}\libpng16.dll"
Type: files; Name: "{app}\UpdateManager.exe"
; The database cache is keyed by WoW build and schema version, not by application
; version. Drop it on upgrade so a changed schema cannot be served from a cache the
; previous build wrote.
Type: files; Name: "{app}\wowdb.sqlite"
Type: files; Name: "{app}\wowdb.sqlite.build"

[Files]
; Everything staged by stage.ps1, layout preserved.
Source: "{#Stage}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Runtime-generated files, so uninstall leaves nothing behind.
Type: filesandordirs; Name: "{app}\userSettings"
Type: files; Name: "{app}\wowdb.sqlite"
Type: files; Name: "{app}\wowdb.sqlite.build"
Type: files; Name: "{app}\listfile.csv.etag"
