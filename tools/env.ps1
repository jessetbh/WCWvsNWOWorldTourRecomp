# Dot-source this to put the project's toolchain on PATH for the current shell:
#   . .\tools\env.ps1
#
# Records the (admin-free) toolchain set up in Phase 2 on this machine. Adjust paths
# if you relocate the toolchain.

$MinGW   = "C:\Users\selki\toolchains\mingw64\bin"                  # g++/gcc 16.1.0 (WinLibs UCRT)
$NinjaDir = "C:\Users\selki\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
$CMakeDir = "C:\Users\selki\AppData\Roaming\Python\Python311\Scripts"  # cmake via pip --user

$env:Path = "$MinGW;$NinjaDir;$CMakeDir;" + $env:Path

Write-Host "Toolchain on PATH:" -ForegroundColor Cyan
foreach ($t in @("gcc","g++","cmake","ninja","python")) {
    $c = Get-Command $t -ErrorAction SilentlyContinue
    if ($c) { Write-Host ("  {0,-7} {1}" -f $t, $c.Source) }
    else    { Write-Host ("  {0,-7} (missing)" -f $t) -ForegroundColor Yellow }
}
