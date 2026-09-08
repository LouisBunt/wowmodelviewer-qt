# Keeps the design system the single source of truth.
#
# The interface used to define its look in about twenty-five per-widget stylesheet strings and
# seven runtime restyle functions across seven files, with some forty raw hex values that
# bypassed the token file entirely. Changing the accent meant finding all of them, and
# near-duplicates (#1c2229 against #1c222a) made that a losing game.
#
# This says so out loud instead of trusting everyone to remember. Three rules:
#
#   1. No raw #rrggbb outside Theme.h / Theme.cpp. Colours come from tok::.
#   2. No setStyleSheet() outside Theme.cpp. The application has one stylesheet; a widget
#      that carries its own cannot be restyled centrally and drifts.
#   3. No point-size fonts (QFont(x, 9)). Points scale with the system DPI while the pixel
#      heights around them do not, which is why the bars clipped at 125 %.
#
#   .\tools\qss-lint.ps1            # report
#   .\tools\qss-lint.ps1 -Strict    # exit 1 on any finding, for CI
#
[CmdletBinding()]
param([switch] $Strict)

$ErrorActionPreference = "Stop"
$root = Split-Path (Split-Path $MyInvocation.MyCommand.Path -Parent) -Parent

# The front-end's own sources. upstream/ is a different project with its own conventions.
# Theme.* IS the design system; GameColours.h holds World of Warcraft's own item-quality
# vocabulary, which the interface may not redefine. Both are exempt by design, not by
# oversight.
$exempt = @("Theme.cpp", "Theme.h", "GameColours.h")
$files = Get-ChildItem -Path $root -Filter *.cpp -File | Where-Object { $_.Name -notin $exempt }
$files += Get-ChildItem -Path $root -Filter *.h -File | Where-Object { $_.Name -notin $exempt }

$targets = @()
foreach ($f in $files) {
  $targets += [pscustomobject]@{ Path = $f.FullName; Name = $f.Name; Full = $true }
}

# The installer's front-end is a second program with its own main(). It installs no
# application stylesheet and sets no application style, so it builds its look from local
# stylesheet strings and its own QFont calls -- rules 2 and 3 cannot apply to it. Rule 1
# can, and is the one that matters here: it includes the same Theme.h, and its colours have
# to come from the same tokens, or the setup ends up looking like the version it replaces.
# It is checked because it drifted exactly that way once and only the compiler noticed.
$setupui = Join-Path $root "installer\setupui\main.cpp"
if (Test-Path $setupui) {
  $targets += [pscustomobject]@{ Path = $setupui; Name = "setupui/main.cpp"; Full = $false }
}

$findings = @()

foreach ($f in $targets) {
  $lineNo = 0
  foreach ($line in Get-Content $f.Path) {
    $lineNo++
    # Comments explain the old values on purpose; they are documentation, not style.
    $code = $line -replace '//.*$', ''
    if ($code -match '#[0-9a-fA-F]{6}') {
      $findings += [pscustomobject]@{ Rule = "raw-colour"; File = $f.Name; Line = $lineNo; Text = $code.Trim() }
    }
    # main.cpp installs the one application stylesheet; that is the point, not a violation.
    if ($f.Full -and ($code -match 'setStyleSheet\s*\(') -and ($code -notmatch 'Theme::sheet\(\)')) {
      $findings += [pscustomobject]@{ Rule = "local-stylesheet"; File = $f.Name; Line = $lineNo; Text = $code.Trim() }
    }
    if ($f.Full -and ($code -match 'QFont\s*\([^)]*,\s*\d+\s*[,)]')) {
      $findings += [pscustomobject]@{ Rule = "point-size-font"; File = $f.Name; Line = $lineNo; Text = $code.Trim() }
    }
    if ($code -match 'tok::k[A-Z]') {
      $findings += [pscustomobject]@{ Rule = "migration-alias"; File = $f.Name; Line = $lineNo; Text = $code.Trim() }
    }
  }
}

if (-not $findings) {
  Write-Output "qss-lint: clean"
  exit 0
}

$findings | Group-Object Rule | Sort-Object Count -Descending | ForEach-Object {
  Write-Output ""
  Write-Output ("{0}: {1}" -f $_.Name, $_.Count)
  $_.Group | Select-Object -First 12 | ForEach-Object {
    Write-Output ("  {0}:{1}  {2}" -f $_.File, $_.Line, ($_.Text.Substring(0, [Math]::Min(90, $_.Text.Length))))
  }
  if ($_.Count -gt 12) { Write-Output ("  ... and {0} more" -f ($_.Count - 12)) }
}

Write-Output ""
Write-Output ("qss-lint: {0} finding(s) in {1} file(s)" -f $findings.Count, ($findings | Select-Object -ExpandProperty File -Unique).Count)
if ($Strict) { exit 1 }
exit 0
