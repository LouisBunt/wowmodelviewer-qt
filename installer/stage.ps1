# Assemble the release payload for better Model Viewer.
#
# Every file is copied from the place that produces it -- build output, the Qt
# installation, the VC redist folder, the tracked bin_support tree -- so the payload
# can be rebuilt from scratch instead of being maintained by hand. The previous
# release folders were hand-copied, which is how they ended up carrying wxWidgets
# leftovers and a zero-byte database cache.
#
# The DLL list is not inherited from the wx package. It is the dependency closure of
# the Qt executable, core.dll, wow.dll and the four plugins, as reported by dumpbin.
# jpeg62.dll and libpng16.dll were in the old package because wxWidgets needed them;
# nothing in this build imports them, so they are gone.
#
#   .\installer\stage.ps1
#   .\installer\stage.ps1 -StageDir D:\tmp\bmv -SkipListfile

[CmdletBinding()]
param(
  # Resolved below rather than here: $PSScriptRoot is not reliably populated during
  # parameter binding under Windows PowerShell 5.1.
  [string] $Root       = "",
  [string] $StageDir   = "",
  # Which upstream build tree core.dll, wow.dll and the plugins come from. Named rather
  # than hardcoded because a CMake cache records the absolute path it was created at: a
  # tree configured on another machine cannot be reconfigured in place, so a working
  # build often lives BESIDE the stale one instead of replacing it.
  [string] $UpstreamBuild = "build-x64",
  [string] $QtDir      = "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64",
  [string] $FbxDir     = "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.9/lib/x64/release",
  [string] $VcRedist   = "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT",
  [string] $Listfile   = "",
  # The listfile is ~148 MB and dominates both staging time and installer size.
  # Skip it while iterating on the package layout.
  [switch] $SkipListfile
)

$ErrorActionPreference = "Stop"

$here = Split-Path $MyInvocation.MyCommand.Path -Parent
if (-not $Root)     { $Root     = Split-Path $here -Parent }
if (-not $StageDir) { $StageDir = Join-Path $here "stage" }

$build    = Join-Path $Root "build\Release"
$upstream = Join-Path $Root "upstream"
$ubuild   = Join-Path $upstream "$UpstreamBuild\Source"
$support  = Join-Path $upstream "bin_support"
if (-not $Listfile) { $Listfile = Join-Path $support "wow\listfile.csv" }

$copied = 0

function Stage {
  param([string] $From, [string] $RelTarget)

  if (-not (Test-Path $From)) {
    throw "missing source: $From`n  (build the project and the upstream tree first, or point the matching -* parameter somewhere else)"
  }
  $to = Join-Path $StageDir $RelTarget
  $dir = Split-Path $to -Parent
  if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
  Copy-Item $From $to -Force
  $script:copied++
}

function StageTree {
  param([string] $From, [string] $RelTarget, [string] $Filter = "*")

  if (-not (Test-Path $From)) { throw "missing source tree: $From" }
  foreach ($f in Get-ChildItem $From -File -Filter $Filter) {
    Stage $f.FullName (Join-Path $RelTarget $f.Name)
  }
}

if (Test-Path $StageDir) { Remove-Item $StageDir -Recurse -Force }
New-Item -ItemType Directory -Path $StageDir -Force | Out-Null

Write-Host "staging into $StageDir"

# --- the application ---------------------------------------------------------
Stage (Join-Path $build "WoWModelViewer-Qt.exe") "WoWModelViewer-Qt.exe"
Stage (Join-Path $ubuild "core\Release\core.dll")     "core.dll"
Stage (Join-Path $ubuild "games\wow\Release\wow.dll") "wow.dll"

# --- exporter and importer plugins -------------------------------------------
# PLUGINMANAGER scans .\plugins relative to the working directory. Without these the
# Export button finds no formats -- which is exactly what the previous zip shipped.
foreach ($p in @("exporters\fbx\Release\fbxexporter.dll",
                 "exporters\obj\Release\objexporter.dll",
                 "importers\armory\Release\armory.dll",
                 "importers\wowhead\Release\wowhead.dll")) {
  Stage (Join-Path $ubuild "plugins\$p") "plugins\$(Split-Path $p -Leaf)"
}
Stage (Join-Path $FbxDir "libfbxsdk.dll") "libfbxsdk.dll"   # fbxexporter.dll imports it

# --- Qt ----------------------------------------------------------------------
foreach ($m in @("Core", "Gui", "Widgets", "Network", "Xml")) {
  Stage (Join-Path $QtDir "bin\Qt5$m.dll") "Qt5$m.dll"
}
# Without platforms\qwindows.dll Qt aborts at startup with "could not find or load
# the Qt platform plugin windows" and no window ever appears.
Stage (Join-Path $QtDir "plugins\platforms\qwindows.dll") "platforms\qwindows.dll"
foreach ($f in @("qjpeg.dll", "qtga.dll")) {
  Stage (Join-Path $QtDir "plugins\imageformats\$f") "imageformats\$f"
}

# --- Visual C++ runtime, app-local -------------------------------------------
# Shipped next to the exe so the installer needs no admin rights and no redist step.
foreach ($f in @("msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
                 "concrt140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
  Stage (Join-Path $VcRedist $f) $f
}

# --- game data definitions ---------------------------------------------------
# database.xml and the CSVs come from the tracked bin_support tree, never from a
# build-staging copy: a stale database.xml silently breaks races and customization.
StageTree (Join-Path $support "wow\12.0") "games\wow\12.0"
StageTree (Join-Path $support "dbd") "dbd"
Stage (Join-Path $support "Encryption\extraEncryptionKeys.csv") "extraEncryptionKeys.csv"

if ($SkipListfile) {
  Write-Warning "listfile.csv skipped -- the staged package will not resolve any model names"
} else {
  Stage $Listfile "listfile.csv"
}

# --- documentation -----------------------------------------------------------
# GPLv3: the licence travels with the binary, not just with the source.
Stage (Join-Path $Root "LICENSE") "LICENSE.txt"
Stage (Join-Path $Root "installer\LIESMICH.txt") "LIESMICH.txt"

# --- report ------------------------------------------------------------------
$bytes = (Get-ChildItem $StageDir -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host ""
Write-Host ("staged {0} files, {1:N1} MB" -f $copied, ($bytes / 1MB))

# Deliberately absent, and why:
#   jpeg62.dll, libpng16.dll   wxWidgets needed these; nothing here imports them
#   libcrypto/libssl-1_1-x64   Qt5Network loads OpenSSL lazily for HTTPS. The armory
#                              and wowhead importers are not reachable from the Qt UI
#                              yet, so nothing makes an HTTPS request. Add them here
#                              when those importers get wired up.
#   wowdb.sqlite               a cache the application builds on first run from dbd\.
#                              Shipping one only risks serving a stale schema.
