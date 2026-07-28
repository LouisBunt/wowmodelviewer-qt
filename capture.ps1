# Capture a window of a process including native child windows (the GL canvas).
# QWidget::grab() cannot see the GL surface and the GL back-buffer read cannot see
# the Qt shell -- PrintWindow with PW_RENDERFULLCONTENT gets both at once.
#
# MainWindowHandle is unreliable for frameless Qt windows (it can return a tiny
# helper window), so enumerate the process's top-level windows and take the largest.
param([int]$ProcessId, [string]$Out)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class Win {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }

  public static List<IntPtr> ForProcess(uint want) {
    var found = new List<IntPtr>();
    EnumWindows((h, p) => {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid == want && IsWindowVisible(h)) found.Add(h);
      return true;
    }, IntPtr.Zero);
    return found;
  }
}
"@

$best = [IntPtr]::Zero; $bestArea = 0; $bw = 0; $bh = 0
foreach ($h in [Win]::ForProcess([uint32]$ProcessId)) {
  $r = New-Object Win+RECT
  [void][Win]::GetWindowRect($h, [ref]$r)
  $w = $r.R - $r.L; $ht = $r.B - $r.T
  if ($w * $ht -gt $bestArea) { $bestArea = $w * $ht; $best = $h; $bw = $w; $bh = $ht }
}

if ($best -eq [IntPtr]::Zero) { Write-Output "kein sichtbares Fenster"; exit 1 }

$bmp = New-Object System.Drawing.Bitmap($bw, $bh)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
[void][Win]::PrintWindow($best, $dc, 2)   # 2 = PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc)
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "gespeichert: $Out ($bw x $bh)"
