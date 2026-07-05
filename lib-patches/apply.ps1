# Reapply the checked-in [wcw fix] patches onto fresh lib/ clones.
# See lib-patches/README.md for the manifest and clone instructions.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

$entries = @(
    @{ Repo = 'lib\N64ModernRuntime';            Base = 'ca568b6ad79b9029d14077f0c3ffa757727c5559'; Patch = 'N64ModernRuntime.patch' },
    @{ Repo = 'lib\N64ModernRuntime\N64Recomp';  Base = '2b6f05688de2abc7d86da5b4a89b84c2c6acbabe'; Patch = 'N64ModernRuntime-N64Recomp.patch' },
    @{ Repo = 'lib\RecompFrontend';              Base = 'b3b7ebb4ec1a8a763c0191486f1b3329f9499a48'; Patch = 'RecompFrontend.patch' },
    @{ Repo = 'lib\rt64';                        Base = 'f647df1a084ae67897dba9806c0d467aa0852894'; Patch = 'rt64.patch' },
    @{ Repo = 'lib\rt64\src\contrib\plume';      Base = '51b1ad443b9f202c5cfc930ae25345d3f2ba7716'; Patch = 'rt64-plume.patch' }
)

$failed = $false
foreach ($e in $entries) {
    $repo = Join-Path $root $e.Repo
    $patch = Join-Path $PSScriptRoot $e.Patch
    if (-not (Test-Path $repo)) {
        Write-Host "MISSING  $($e.Repo) - clone it first (see lib-patches/README.md)" -ForegroundColor Red
        $failed = $true
        continue
    }
    $head = (git -C $repo rev-parse HEAD).Trim()
    if ($head -ne $e.Base) {
        Write-Host "WARNING  $($e.Repo) is at $head, expected $($e.Base) - patch may not apply" -ForegroundColor Yellow
    }
    git -C $repo apply --reverse --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "ALREADY  $($e.Repo) ($($e.Patch) present)"
        continue
    }
    git -C $repo apply --check $patch 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAILED   $($e.Repo): $($e.Patch) does not apply cleanly - resolve manually" -ForegroundColor Red
        $failed = $true
        continue
    }
    git -C $repo apply $patch
    Write-Host "APPLIED  $($e.Repo) from $($e.Patch)" -ForegroundColor Green
}

if ($failed) { exit 1 }
Write-Host 'Done.'
