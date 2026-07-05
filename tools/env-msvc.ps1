# Dot-source this to load the Visual Studio 2022 Build Tools environment (clang-cl, cl,
# the bundled CMake + Ninja, the Windows SDK) into the current shell:
#   . .\tools\env-msvc.ps1
# Use this for building the actual port (RT64 needs clang-cl + the Windows SDK).
# (tools/env.ps1 is the separate MinGW toolchain used only to build the N64Recomp tool.)

$vsPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools"
$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }

# Import the environment that vcvars64.bat sets up.
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] }
}

Write-Host "VS Build Tools environment loaded:" -ForegroundColor Cyan
foreach ($t in @("clang-cl", "cl", "cmake", "ninja", "lld-link")) {
    $c = Get-Command $t -ErrorAction SilentlyContinue
    if ($c) { Write-Host ("  {0,-10} {1}" -f $t, $c.Source) }
    else    { Write-Host ("  {0,-10} (missing)" -f $t) -ForegroundColor Yellow }
}
