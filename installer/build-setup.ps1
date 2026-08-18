# Build the complete release setup: payload, Qt setup front-end, Inno package.
#
# One command instead of three that must agree with each other. The staged file count
# and payload size are measured here and handed to ISCC, which hands them to the UI --
# the progress bar's denominator is a measurement, not a guess that drifts.
#
#   .\installer\build-setup.ps1
#   .\installer\build-setup.ps1 -SkipStage        # payload unchanged, UI/installer work
#
# Prerequisites: the application build (build\Release) and the upstream tree, as for
# stage.ps1; CMake + MSVC for the UI; Inno Setup 6.

[CmdletBinding()]
param(
  [string] $QtDir = "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64",
  [string] $VcRedist = "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT",
  [string] $Iscc = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
  [switch] $SkipStage
)

$ErrorActionPreference = "Stop"
$here = Split-Path $MyInvocation.MyCommand.Path -Parent
$root = Split-Path $here -Parent

# --- 1. the payload ----------------------------------------------------------
# A .ps1 does not set $LASTEXITCODE; with $ErrorActionPreference = Stop its own
# throws abort this script, which is the check.
if (-not $SkipStage) {
  & (Join-Path $here "stage.ps1") -QtDir $QtDir -VcRedist $VcRedist
}
$stage = Join-Path $here "stage"
if (-not (Test-Path $stage)) { throw "no staged payload -- run without -SkipStage first" }

# --- 2. the setup front-end --------------------------------------------------
$uiBuild = Join-Path $root "build-setupui"
cmake -S (Join-Path $root "installer/setupui") -B $uiBuild -G "Visual Studio 17 2022" -A x64 "-DQT_LOCATION=$QtDir"
if ($LASTEXITCODE -ne 0) { throw "setupui: cmake configure failed" }
cmake --build $uiBuild --config Release
if ($LASTEXITCODE -ne 0) { throw "setupui: build failed" }

# What InitializeSetup extracts to {tmp}: the UI, its Qt closure, the CRT it was
# compiled against, and the licence text its second page displays. The relative
# layout (platforms\) survives extraction, which Qt requires.
$uiStage = Join-Path $here "setupui-stage"
if (Test-Path $uiStage) { Remove-Item $uiStage -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $uiStage "platforms") -Force | Out-Null

Copy-Item (Join-Path $uiBuild "Release\setupui.exe") $uiStage
foreach ($m in @("Core", "Gui", "Widgets")) {
  Copy-Item (Join-Path $QtDir "bin/Qt5$m.dll") $uiStage
}
Copy-Item (Join-Path $QtDir "plugins/platforms/qwindows.dll") (Join-Path $uiStage "platforms")
foreach ($f in @("msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
                 "vcruntime140.dll", "vcruntime140_1.dll")) {
  Copy-Item (Join-Path $VcRedist $f) $uiStage
}
Copy-Item (Join-Path $root "LICENSE") (Join-Path $uiStage "LICENSE.txt")

# --- 3. measure, then compile the installer ----------------------------------
$files = Get-ChildItem $stage -Recurse -File
$totalFiles = $files.Count
$stageMb = [int][math]::Round(($files | Measure-Object Length -Sum).Sum / 1MB)
Write-Host ("payload: {0} files, {1} MB" -f $totalFiles, $stageMb)

if (-not (Test-Path $Iscc)) { throw "Inno Setup 6 not found at $Iscc -- pass -Iscc" }
& $Iscc "/DTotalFiles=$totalFiles" "/DStageMB=$stageMb" (Join-Path $here "MVMidnight.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }

$out = Get-ChildItem (Join-Path $here "dist") -Filter "MV-Midnight-Setup-*.exe" |
       Sort-Object LastWriteTime | Select-Object -Last 1
Write-Host ""
Write-Host ("setup: {0} ({1:N1} MB)" -f $out.FullName, ($out.Length / 1MB))
