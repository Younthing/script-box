#requires -Version 5.1
# Manual arg parser to avoid host/switch parsing quirks.

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Defaults
$AppVersion = $null
$QtDir = $env:QTDIR
$BuildDir = "build"
$FeedUrl = "https://github.com/Younthing/script-box/releases/latest/download/update.json"
$Notes = ""
$SkipBuild = $false
$DoTag = $false
$DoPush = $false
$DoRelease = $false

function Resolve-QtDir {
    param([string]$Hint)
    if (![string]::IsNullOrWhiteSpace($Hint) -and (Test-Path $Hint)) {
        return (Resolve-Path $Hint).Path
    }
    $candidate = "D:\app\qt\6.10.1\mingw_64"
    if (Test-Path $candidate) { return $candidate }
    throw "Qt directory not found. Set -QtDir or QTDIR."
}

function Run-Cmd {
    param([string]$Cmd, [string]$ErrorMsg)
    Write-Host "==> $Cmd"
    $oldPreference = $global:ErrorActionPreference
    $global:ErrorActionPreference = "Stop"
    try {
        Invoke-Expression $Cmd
    } catch {
        throw "$ErrorMsg : $($_.Exception.Message)"
    } finally {
        $global:ErrorActionPreference = $oldPreference
    }
}

# Manual arg parsing
$i = 0
while ($i -lt $args.Count) {
    switch -Regex ($args[$i]) {
        '^-AppVersion$' { $AppVersion = $args[$i + 1]; $i += 2; continue }
        '^-QtDir$'      { $QtDir = $args[$i + 1]; $i += 2; continue }
        '^-BuildDir$'   { $BuildDir = $args[$i + 1]; $i += 2; continue }
        '^-FeedUrl$'    { $FeedUrl = $args[$i + 1]; $i += 2; continue }
        '^-Notes$'      { $Notes = $args[$i + 1]; $i += 2; continue }
        '^-SkipBuild$'  { $SkipBuild = $true; $i += 1; continue }
        '^-Tag$'        { $DoTag = $true; $i += 1; continue }
        '^-Push$'       { $DoPush = $true; $i += 1; continue }
        '^-Release$'    { $DoRelease = $true; $i += 1; continue }
        default         { throw "Unknown argument: $($args[$i])" }
    }
}

if ([string]::IsNullOrWhiteSpace($AppVersion)) {
    throw "AppVersion is required. Usage: pwsh -File scripts/release.ps1 -AppVersion 0.0.2 [-QtDir ...] [-FeedUrl ...] [-SkipBuild] [-Tag] [-Push] [-Release]"
}

$qt = Resolve-QtDir -Hint $QtDir
$GitTag = "v$AppVersion"
$zipPath = "dist/ScriptToolbox.zip"
$updatePath = "dist/update.json"
$zipUrl = "https://github.com/Younthing/script-box/releases/download/$GitTag/ScriptToolbox.zip"
if ([string]::IsNullOrWhiteSpace($Notes)) {
    $Notes = "Release $AppVersion"
}

Write-Host "Version: $AppVersion"
Write-Host "QtDir: $qt"
Write-Host "BuildDir: $BuildDir"
Write-Host "FeedUrl: $FeedUrl"
Write-Host "ZipUrl: $zipUrl"
Write-Host "SkipBuild: $SkipBuild  Tag: $DoTag  Push: $DoPush  Release: $DoRelease"

if (-not $SkipBuild) {
    Run-Cmd "cmake -S . -B $BuildDir -G `"MinGW Makefiles`" -D CMAKE_BUILD_TYPE=Release -D CMAKE_PREFIX_PATH=$qt -D APP_VERSION=$AppVersion -D UPDATE_FEED_URL=$FeedUrl" "Configure failed"
    Run-Cmd "cmake --build $BuildDir --config Release" "Build failed"
}

Run-Cmd "pwsh -File scripts/package.ps1 -QtDir $qt -BuildDir $BuildDir -OutputDir dist/ScriptToolbox -Zip -SkipBuild" "Package failed"
Run-Cmd "pwsh -File scripts/make-update-feed.ps1 -Version $AppVersion -ZipUrl $zipUrl -Notes `"$Notes`"" "Update feed generation failed"

if ($DoTag) {
    Run-Cmd "git tag $GitTag" "git tag failed"
    if ($DoPush) {
        Run-Cmd "git push origin $GitTag" "git push tag failed"
    }
}

if ($DoRelease) {
    Run-Cmd "gh release create $GitTag $zipPath $updatePath --title `"Script Toolbox $GitTag`" --notes `"$Notes`"" "gh release failed"
}

Write-Host "Done. Artifacts:"
Write-Host "  $zipPath"
Write-Host "  $updatePath"
