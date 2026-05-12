# Usage: .\tools\png_to_header.ps1
# Run from the repo root (Claude-VST-2-stable\)
param(
    [string]$Src = "Assets\knob_strip.png",
    [string]$Dst = "Source\KnobStripData.h"
)

$data = [System.IO.File]::ReadAllBytes($Src)
$writer = [System.IO.StreamWriter]::new($Dst)

$writer.WriteLine("// Auto-generated — do not edit. Run tools/png_to_header.ps1 to regenerate.")
$writer.WriteLine("#pragma once")
$writer.WriteLine("#include <cstddef>")
$writer.WriteLine("")
$writer.WriteLine("static const unsigned char knob_strip_png[] = {")

$sb = [System.Text.StringBuilder]::new(128)
for ($i = 0; $i -lt $data.Length; $i++) {
    if ($i % 16 -eq 0) { [void]$sb.Append("    ") }
    [void]$sb.Append(("0x{0:x2}," -f $data[$i]))
    if ($i % 16 -eq 15 -or $i -eq $data.Length - 1) {
        $writer.WriteLine($sb.ToString())
        [void]$sb.Clear()
    }
}

$writer.WriteLine("};")
$writer.WriteLine("static const size_t knob_strip_png_size = $($data.Length);")
$writer.Close()

Write-Host "Done: $($data.Length) bytes -> $Dst"
