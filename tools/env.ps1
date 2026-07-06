# Dot-source this to put the project's toolchain on PATH for the current shell:
#   . .\tools\env.ps1
#
# Puts the MinGW/CMake/Ninja toolchain (used to build N64Recomp and run gen_symbols)
# on PATH. Set WCW_MINGW / WCW_NINJA_DIR / WCW_CMAKE_DIR to your install locations if
# the tools aren't already on PATH (defaults: a portable WinLibs UCRT MinGW, winget
# Ninja, pip --user CMake).

$MinGW    = if ($env:WCW_MINGW)     { $env:WCW_MINGW }     else { "$env:USERPROFILE\toolchains\mingw64\bin" }
$NinjaDir = if ($env:WCW_NINJA_DIR) { $env:WCW_NINJA_DIR } else { "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe" }
$CMakeDir = if ($env:WCW_CMAKE_DIR) { $env:WCW_CMAKE_DIR } else { "$env:APPDATA\Python\Python311\Scripts" }

foreach ($d in @($MinGW, $NinjaDir, $CMakeDir)) {
    if (Test-Path $d) { $env:Path = "$d;" + $env:Path }
}

Write-Host "Toolchain on PATH:" -ForegroundColor Cyan
foreach ($t in @("gcc","g++","cmake","ninja","python")) {
    $c = Get-Command $t -ErrorAction SilentlyContinue
    if ($c) { Write-Host ("  {0,-7} {1}" -f $t, $c.Source) }
    else    { Write-Host ("  {0,-7} (missing)" -f $t) -ForegroundColor Yellow }
}
