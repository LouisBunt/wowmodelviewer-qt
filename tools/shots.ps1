# Headless screenshot matrix -- the proof that a UI change did what it claims.
#
# Every case starts the staged application, waits for a marker in the trace (so the wait is on
# the application's own progress, not on a guessed number of seconds), photographs the window
# with capture.ps1 (PrintWindow + PW_RENDERFULLCONTENT: GL canvas AND Qt overlays in one image)
# and kills it again. Nothing here clicks: what cannot be reached from the command line cannot
# be photographed, which is the rule that keeps the CLI flags honest.
#
#   .\tools\shots.ps1 -Out D:\shots\before
#   .\tools\shots.ps1 -Out D:\shots\after -Only tab3,orc
#
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string] $Out,
  # Subset of case names, comma separated. Empty = all.
  [string] $Only = "",
  [string] $Stage = "",
  [string] $Wow = "C:\Program Files (x86)\World of Warcraft",
  [int] $TimeoutSec = 180
)

$ErrorActionPreference = "Stop"
$here = Split-Path $MyInvocation.MyCommand.Path -Parent
$root = Split-Path $here -Parent
if (-not $Stage) { $Stage = Join-Path $root "installer\stage" }
$exe = Join-Path $Stage "WoWModelViewer-Qt.exe"
$capture = Join-Path $root "capture.ps1"
$trace = Join-Path $Stage "userSettings\qt-frontend-trace.txt"
if (-not (Test-Path $exe)) { throw "not staged: $exe" }
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# name = the file that gets written; args = command line; marker = trace line to wait for.
# The markers are the application's own trace strings; a case that never prints its marker is
# a failure of the case, not of the wait.
$cases = @(
  @{ name = "empty";    args = @();                                        marker = "populateTree returned" }
  @{ name = "tab0";     args = @("917116", "--tab", "0");                  marker = "inspector tab set to 0" }
  @{ name = "tab1";     args = @("917116", "--tab", "1");                  marker = "inspector tab set to 1" }
  @{ name = "tab2";     args = @("917116", "--tab", "2");                  marker = "inspector tab set to 2" }
  @{ name = "tab3";     args = @("917116", "--tab", "3");                  marker = "inspector tab set to 3" }
  @{ name = "cat-items";args = @("--category", "3");                       marker = "category set to 3" }
  @{ name = "cat-npcs"; args = @("--category", "4");                       marker = "category set to 4" }
  @{ name = "chromie";  args = @("--npc", "Chromie", "--tab", "0");        marker = "inspector tab set to 0" }
  # The long-name case: a plate set whose German item names are the ones that used to be cut
  # off mid-word in the equipment rows.
  @{ name = "orc-equip"; args = @("917116", "--equip", "18817,16963,16961,16959,16966,16965,16962,16964,16960", "--tab", "0"); marker = "inspector tab set to 0" }
  @{ name = "item-solo"; args = @("--item-solo", "19019", "--tab", "3");   marker = "inspector tab set to 3" }
)

if ($Only) {
  $want = $Only.Split(",") | ForEach-Object { $_.Trim() }
  $cases = $cases | Where-Object { $want -contains $_.name }
}

foreach ($c in $cases) {
  $png = Join-Path $Out "$($c.name).png"
  Write-Output "--- $($c.name)"
  Remove-Item $trace -ErrorAction SilentlyContinue
  $argList = @('"' + $Wow + '"') + $c.args
  $p = Start-Process -FilePath $exe -WorkingDirectory $Stage -ArgumentList $argList -PassThru
  $deadline = (Get-Date).AddSeconds($TimeoutSec)
  $seen = $false
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) { break }
    $t = Get-Content $trace -ErrorAction SilentlyContinue
    if ($t -and ($t -match [regex]::Escape($c.marker))) { $seen = $true; break }
  }
  if (-not $seen) {
    Write-Output "    marker not seen: $($c.marker)"
  } else {
    # The marker only says the flag was applied; the first frames still have to be drawn.
    Start-Sleep -Seconds 6
    & $capture -ProcessId $p.Id -Out $png
  }
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
  Start-Sleep -Seconds 1
}

Write-Output ""
Get-ChildItem $Out -Filter *.png | Select-Object Name, Length | Format-Table -AutoSize
