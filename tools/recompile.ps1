# Regenerate symbols from the disassembly and run N64Recomp on the base game.
# Usage:  . .\tools\env.ps1 ;  .\tools\recompile.ps1
# Note: don't use ErrorActionPreference=Stop here — N64Recomp writes informational
# messages (e.g. the overlay "Ambiguous jal" note) to stderr, which would otherwise
# abort the script even though it exits 0. We check $LASTEXITCODE explicitly instead.
$root = Split-Path $PSScriptRoot -Parent

# 1. (Re)generate syms/dump.toml from the splat disassembly (includes overlays).
python "$root\tools\gen_symbols.py" --overlays

# 2. Ensure the ROM is where wcw.toml expects it.
if (-not (Test-Path "$root\wcw.z64")) {
    Copy-Item "$root\disasm\wcw.z64" "$root\wcw.z64"
}

# 3. Run the recompiler. Output lands in RecompiledFuncs/.
Push-Location $root
& "$root\N64Recomp.exe" wcw.toml
$code = $LASTEXITCODE
Pop-Location

if ($code -eq 0) {
    $c = Get-ChildItem "$root\RecompiledFuncs" -Filter *.c
    Write-Host "OK - recompiled to $($c.Count) C files, $([math]::Round((($c|Measure-Object Length -Sum).Sum)/1MB,2)) MB" -ForegroundColor Green
} else {
    Write-Host "N64Recomp failed (exit $code)" -ForegroundColor Red
}
exit $code
