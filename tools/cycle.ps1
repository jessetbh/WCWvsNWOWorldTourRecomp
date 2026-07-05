# Dev helper: regen symbols -> N64Recomp -> clang-cl build -> run -> symbolize crash frames.
# Usage:  . .\tools\cycle.ps1   (run from repo root; loads both toolchains itself)
$ErrorActionPreference = "Continue"  # native-tool stderr (N64Recomp [Info]) must not abort
$root = "C:\Users\selki\depot\WcwNwoWorldTour"
$build = "$root\build-msvc"

# --- Phase 1: regen + recompile (MinGW/python env) ---
& "$root\tools\env.ps1" | Out-Null
Push-Location $root
python .\tools\gen_symbols.py --overlays --data | Select-Object -Last 1
& "$root\N64Recomp.exe" wcw.toml *>$null
if ($LASTEXITCODE -ne 0) { Write-Host "N64Recomp FAILED ($LASTEXITCODE)"; Pop-Location; return }
Write-Host "recompile OK"
Pop-Location

# --- Phase 2: build (MSVC env) ---
& "$root\tools\env-msvc.ps1" | Out-Null
$blog = cmake --build $build --target WCWRecompiled 2>&1
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED:"; $blog | Select-Object -Last 25; return }
Write-Host "build OK"

# --- Phase 3: run with timeout, capture merged output ---
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = "cmd.exe"; $psi.Arguments = "/c `".\WCWRecompiled.exe`" > bt.txt 2>&1"
$psi.WorkingDirectory = $build; $psi.UseShellExecute = $false
$p = [System.Diagnostics.Process]::Start($psi)
if ($p.WaitForExit(20000)) { Write-Host ("exit=0x{0:X8}" -f $p.ExitCode) }
else { Write-Host "STILL RUNNING after 20s (good sign) - killing"; $p.Kill() }

Write-Host "--- tail ---"
Get-Content "$build\bt.txt" | Select-Object -Last 20

# --- Phase 4: symbolize WCWRecompiled.exe frames ---
$frames = Select-String -Path "$build\bt.txt" -Pattern '#\s*\d+\s+WCWRecompiled\.exe\s+\+0x([0-9A-Fa-f]+)' | ForEach-Object { [Convert]::ToInt64($_.Matches[0].Groups[1].Value,16) }
if ($frames) {
    $base = 0x140000000
    $syms = New-Object System.Collections.Generic.List[object]
    foreach ($l in Get-Content "$build\WCWRecompiled.map") {
        if ($l -match '^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{16})\s') {
            $syms.Add([pscustomobject]@{ Rva = [Convert]::ToUInt64($matches[2],16) - $base; Name = $matches[1] })
        }
    }
    $sorted = $syms | Sort-Object Rva
    Write-Host "--- symbolized frames ---"
    foreach ($t in $frames) {
        $below = $null
        foreach ($s in $sorted) { if ($s.Rva -le $t) { $below = $s } else { break } }
        if ($below) { Write-Host ("  0x{0:X7}  =>  {1}+0x{2:X}" -f $t, $below.Name, ($t - $below.Rva)) }
    }
}
