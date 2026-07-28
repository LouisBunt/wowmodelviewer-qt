# Capture a top-level window including native child windows (the GL canvas).
# QWidget::grab() cannot see the GL surface, and the GL back-buffer read cannot see
# the Qt shell -- PrintWindow with PW_RENDERFULLCONTENT gets both at once.
param([int]$ProcessId, [string]$Out)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@

$p = Get-Process -Id $ProcessId
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Output "kein Fenster"; exit 1 }

$r = New-Object Win+RECT
[void][Win]::GetWindowRect($h, [ref]$r)
$w = $r.R - $r.L
$ht = $r.B - $r.T

$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[void][Win]::PrintWindow($h, $dc, 2)   # 2 = PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc)
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "gespeichert: $Out ($w x $ht)"
