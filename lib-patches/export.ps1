# Re-export the lib/ working-tree diffs into lib-patches/*.patch.
# Run after changing anything under lib/ so the checked-in patches stay current.
# New (untracked) files must be registered with `git add -N <file>` in their repo
# to be included — this script does that for known ones.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

# Known new files (intent-to-add so they appear in git diff).
git -C lib\N64ModernRuntime add -N librecomp/src/si.cpp

# cmd-level redirection writes the diff bytes verbatim (PowerShell pipelines would
# re-encode and CRLF-convert, corrupting the patches).
cmd /c "git -C lib\N64ModernRuntime diff --ignore-submodules=all > lib-patches\N64ModernRuntime.patch"
cmd /c "git -C lib\N64ModernRuntime\N64Recomp diff > lib-patches\N64ModernRuntime-N64Recomp.patch"
cmd /c "git -C lib\RecompFrontend diff > lib-patches\RecompFrontend.patch"
cmd /c "git -C lib\rt64 diff --ignore-submodules=all > lib-patches\rt64.patch"
cmd /c "git -C lib\rt64\src\contrib\plume diff > lib-patches\rt64-plume.patch"

# Warn if any repo has untracked files this script doesn't know about (they would be lost).
foreach ($repo in 'lib\N64ModernRuntime', 'lib\RecompFrontend', 'lib\rt64') {
    $untracked = git -C $repo ls-files --others --exclude-standard
    if ($untracked) {
        Write-Host "WARNING: untracked files in $repo not covered by patches:" -ForegroundColor Yellow
        $untracked | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
        Write-Host "  Add a 'git add -N' line above for them and re-run." -ForegroundColor Yellow
    }
}

Get-ChildItem lib-patches\*.patch | Format-Table Name, Length
Write-Host 'Exported. Remember to commit lib-patches/.'
