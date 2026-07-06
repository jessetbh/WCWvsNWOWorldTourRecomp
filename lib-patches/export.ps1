# Re-export the lib/ diffs-vs-upstream into lib-patches/*.patch.
#
# Since the 2026-07-05 restructure, lib/* are submodules of the jessetbh forks and the
# [wcw fix] changes are COMMITS on each fork's `wcw` branch — the forks are canonical.
# These patches remain as a human-reviewable record of exactly what differs from
# upstream, so this script diffs <upstream base>..HEAD (not the working tree).
# Run after committing anything new to a fork's wcw branch.
#
# The base SHAs are the pinned upstream commits each wcw branch is based on; update
# them if the forks are ever rebased onto newer upstream.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$bases = @{
    'lib\N64ModernRuntime'           = 'ca568b6'
    'lib\N64ModernRuntime\N64Recomp' = '2b6f056'
    'lib\RecompFrontend'             = 'b3b7ebb'
    'lib\rt64'                       = 'f647df1'
    'lib\rt64\src\contrib\plume'     = '51b1ad4'
}
$outs = @{
    'lib\N64ModernRuntime'           = 'N64ModernRuntime.patch'
    'lib\N64ModernRuntime\N64Recomp' = 'N64ModernRuntime-N64Recomp.patch'
    'lib\RecompFrontend'             = 'RecompFrontend.patch'
    'lib\rt64'                       = 'rt64.patch'
    'lib\rt64\src\contrib\plume'     = 'rt64-plume.patch'
}

foreach ($repo in $bases.Keys) {
    # Warn about uncommitted changes — they are NOT captured (commit to the fork first).
    $dirty = git -C $repo status --porcelain --ignore-submodules=all
    if ($dirty) {
        Write-Host "WARNING: $repo has uncommitted changes; commit them to the fork's wcw branch first:" -ForegroundColor Yellow
        $dirty | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
    }
    # cmd-level redirection writes the diff bytes verbatim (PowerShell pipelines would
    # re-encode and CRLF-convert, corrupting the patches).
    cmd /c "git -C $repo diff --ignore-submodules=all $($bases[$repo]) HEAD > lib-patches\$($outs[$repo])"
}

Get-ChildItem lib-patches\*.patch | ForEach-Object { Write-Host ("  {0}  {1} bytes" -f $_.Name, $_.Length) }
Write-Host 'Exported. Remember to commit lib-patches/.'
