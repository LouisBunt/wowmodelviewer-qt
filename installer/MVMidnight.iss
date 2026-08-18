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
; The same name WITHOUT the colon, for everywhere a path is built from it. Windows forbids
; \ / : * ? " < > | in folder and file names, so the colon made Setup reject the Start-menu
; group and both shortcut files -- the installation could not be completed at all. AppName
; keeps the colon; that one is only ever displayed.
#define MyAppNameFs "ModelViewer Midnight"
#define MyAppVersion "1.8.0"
#define MyAppVersionNumeric "1.8.0.0"

; Passed by installer\build-setup.ps1, which counts the staged payload. The UI divides
; its progress bar by TotalFiles and prints StageMB on the welcome page; both are
; display values only and the fallbacks merely make a bare ISCC run usable.
#ifndef TotalFiles
  #define TotalFiles "1300"
#endif
#ifndef StageMB
  #define StageMB "450"
#endif
#define MyAppPublisher "Skogdesign"
#define MyAppExeName "WoWModelViewer-Qt.exe"
#define MyAppURL "https://github.com/LouisBunt/wowmodelviewer-qt"
#define Stage "stage"
#define IconsDir "..\upstream\bin_support\Icons"

[Setup]
; Own AppId. This used to reuse the one belonging to WoW Model Viewer Midnight, so the
; setup silently took over THAT product's installation. That was defensible while this was
; a private build replacing its own predecessor; as a public release under a different name
; and publisher it is not -- a stranger installing it would have found another program's
; installation overwritten and its executable deleted, without ever being asked.
;
; Consequence for anyone who installed a build before 1.7.0: this one lands in its own
; directory, and the old entry stays in the program list until it is uninstalled.
AppId={{7C3E9A41-58D2-4B6E-9E17-3F84A5C0D2B9}
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
DefaultGroupName={#MyAppNameFs}
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

[Setup]
; No language dialog: it would appear BEFORE InitializeSetup and therefore in front of
; the Qt front-end, asking a question the front-end never needs answered. The engine
; picks the language from Windows; it only surfaces in the classic-wizard fallback and
; in engine-level error boxes.
ShowLanguageDialog=no

[Messages]
; The directory page offers %LOCALAPPDATA%\Programs, because PrivilegesRequired=lowest
; resolves {autopf} there. Browsing to C:\Program Files from that page looks like a normal
; choice and then fails on permissions -- and the program writes its config, log and
; database cache next to itself, so it genuinely needs a writable folder. Say so up front.
german.SelectDirLabel3=Setup installiert [name] in den folgenden Ordner. Das Programm legt seine Einstellungen, das Protokoll und den Datenbank-Cache neben sich ab und braucht deshalb einen beschreibbaren Ordner — ohne Administratorrechte ist das der Benutzerordner, nicht "C:\Programme".
english.SelectDirLabel3=Setup will install [name] into the following folder. The program keeps its settings, log and database cache next to itself and therefore needs a writable folder — without administrator rights that means your user folder, not "C:\Program Files".

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; Offered, never assumed. Deleting another installation's files is the user's call, and it
; only makes sense when this is deliberately installed over an older WoW Model Viewer.
Name: "cleanwx"; Description: "Dateien einer vorhandenen WoW-Model-Viewer-Installation im Zielordner entfernen"; Flags: unchecked

[InstallDelete]
; Only when the user ticked the task above. A stale wowmodelviewer.exe beside the new build
; is an invitation to start the wrong one and report bugs against it -- but that is a reason
; to OFFER the cleanup, not to carry it out unasked on files this setup did not install.
Type: files; Name: "{app}\wowmodelviewer.exe"; Tasks: cleanwx
Type: files; Name: "{app}\wxbase32u_*.dll"; Tasks: cleanwx
Type: files; Name: "{app}\wxmsw32u_*.dll"; Tasks: cleanwx
Type: files; Name: "{app}\jpeg62.dll"; Tasks: cleanwx
Type: files; Name: "{app}\libpng16.dll"; Tasks: cleanwx
Type: files; Name: "{app}\UpdateManager.exe"; Tasks: cleanwx
; The database cache is keyed by WoW build and schema version, not by application
; version. Drop it on upgrade so a changed schema cannot be served from a cache the
; previous build wrote.
Type: files; Name: "{app}\wowdb.sqlite"
Type: files; Name: "{app}\wowdb.sqlite.build"

[Files]
; Everything staged by stage.ps1, layout preserved.
Source: "{#Stage}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; The Qt setup front-end (installer\setupui, staged by build-setup.ps1). Never installed:
; [Code] extracts it to {tmp} and shows it INSTEAD of the classic wizard, and the UI then
; re-runs this same exe with /VERYSILENT to do the actual work. dontcopy files keep their
; relative paths under {tmp}, which is how platforms\qwindows.dll ends up where Qt looks.
Source: "setupui-stage\*"; Flags: dontcopy recursesubdirs

[Icons]
; MyAppNameFs, not MyAppName: these are file names on disk, and the colon is illegal there.
Name: "{group}\{#MyAppNameFs}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{#MyAppNameFs} deinstallieren"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppNameFs}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
{ The two-phase arrangement. Phase 1 is the exe the user double-clicks: it extracts the
  Qt front-end and shows it instead of the classic wizard, then exits. Phase 2 is the
  same exe, re-run BY that front-end with /VERYSILENT -- WizardSilent is true, the code
  below steps aside, and the stock Inno engine installs under the UI's progress display.

  Every failure path falls back to Result := True, i.e. to the classic wizard: an
  installer whose pretty front cannot start must still be able to install.

  Known cost of the trick: the DOUBLE-CLICKED process exits with code 1, because to
  Inno an InitializeSetup returning False is an aborted setup. The process that did
  the installing reports correctly -- so tools that script this installer should call
  it with /VERYSILENT themselves (bypassing the front-end entirely), which is what
  package managers do anyway. }
function InitializeSetup(): Boolean;
var
  R: Integer;
  Ui: String;
begin
  Result := True;
  if WizardSilent then
    Exit;

  try
    ExtractTemporaryFiles('{tmp}\*');
  except
    Log('setupui: extraction failed, falling back to the classic wizard');
    Exit;
  end;

  Ui := ExpandConstant('{tmp}\setupui.exe');
  if not FileExists(Ui) then
  begin
    Log('setupui: not in the package, classic wizard');
    Exit;
  end;

  { ewWaitUntilTerminated: this instance owns the temp dir and must outlive the UI
    running from it. The silent re-run is a second process of the same exe; without a
    SetupMutex there is nothing for the two to fight over. }
  if Exec(Ui,
          '--inner "' + ExpandConstant('{srcexe}') + '"'
            + ' --totalfiles {#TotalFiles} --sizemb {#StageMB} --version {#MyAppVersion}',
          ExpandConstant('{tmp}'), SW_SHOW, ewWaitUntilTerminated, R) then
    Result := False   { the UI ran the install (or the user cancelled) -- do not }
  else                { show a second wizard behind it }
    Log('setupui: could not be started, classic wizard');
end;

[UninstallDelete]
; Runtime-generated files, so uninstall leaves nothing behind.
Type: filesandordirs; Name: "{app}\userSettings"
Type: files; Name: "{app}\wowdb.sqlite"
Type: files; Name: "{app}\wowdb.sqlite.build"
Type: files; Name: "{app}\listfile.csv.etag"
