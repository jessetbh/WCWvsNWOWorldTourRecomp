# Build the standalone experiment harness. Requires the toolchain on PATH:
#   . .\tools\env.ps1 ; .\run\build.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$run  = $PSScriptRoot
$rf   = Join-Path $root "RecompiledFuncs"
$obj  = Join-Path $run "obj"
New-Item -ItemType Directory -Force -Path $obj | Out-Null

$inc = @("-I$run", "-I$rf")
# -finstrument-functions hooks every recompiled function entry so the harness watchdog
# can report where execution is (e.g. an infinite hardware-poll loop).
$cflags = @("-O0", "-g", "-DNDEBUG", "-fno-strict-aliasing", "-w", "-finstrument-functions") + $inc

$sw = [System.Diagnostics.Stopwatch]::StartNew()

# Compile the generated recompiled functions (C) in parallel jobs.
$cfiles = Get-ChildItem $rf -Filter "funcs_*.c"
Write-Host "Compiling $($cfiles.Count) recompiled C files..."
$jobs = @()
foreach ($f in $cfiles) {
    $o = Join-Path $obj ($f.BaseName + ".o")
    $jobs += Start-Job -ScriptBlock {
        param($gcc, $cflags, $src, $out)
        & $gcc $cflags -c $src -o $out 2>&1
    } -ArgumentList @((Get-Command gcc).Source, $cflags, $f.FullName, $o)
}
$failed = 0
foreach ($j in $jobs) {
    $out = Receive-Job -Job $j -Wait
    if ($j.State -ne 'Completed' -or $LASTEXITCODE -ne 0) { }
    if ($out) { $failed++; Write-Host $out -ForegroundColor Yellow }
    Remove-Job $j
}

# Compile harness (C++).
Write-Host "Compiling harness.cpp..."
& g++ -O0 -g -DNDEBUG -fno-strict-aliasing -std=c++17 -w $inc -c (Join-Path $run "harness.cpp") -o (Join-Path $obj "harness.o")

# Link.
Write-Host "Linking..."
$objs = Get-ChildItem $obj -Filter "*.o" | ForEach-Object { $_.FullName }
& g++ $objs -o (Join-Path $run "wcw_harness.exe")

$sw.Stop()
if (Test-Path (Join-Path $run "wcw_harness.exe")) {
    Write-Host ("OK - built run/wcw_harness.exe in {0:N1}s" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Green
} else {
    Write-Host "Build failed" -ForegroundColor Red
}
