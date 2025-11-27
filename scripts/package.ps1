#requires -Version 5.1
param(
    [string]$SourceDir = (Resolve-Path ".").Path,
    [string]$BuildDir = "build",
    [string]$QtDir = $env:QTDIR,
    [string]$OutputDir = "dist/ScriptToolbox",
    [switch]$SkipBuild = $false,
    [switch]$Zip = $false
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-QtDir {
    param([string]$Hint)
    if (![string]::IsNullOrWhiteSpace($Hint) -and (Test-Path $Hint)) {
        return (Resolve-Path $Hint).Path
    }
    # Fallback to a common Qt install location (adjust if needed)
    $candidate = "D:\app\qt\6.10.1\mingw_64"
    if (Test-Path $candidate) { return $candidate }
    throw "Qt directory not found. Please set -QtDir or QTDIR."
}

$QtDir = Resolve-QtDir -Hint $QtDir
$BuildDir = Resolve-Path $BuildDir -Relative
$SourceDir = Resolve-Path $SourceDir

Write-Host "Using QtDir: $QtDir"
Write-Host "BuildDir: $BuildDir"
Write-Host "SourceDir: $SourceDir"

if (-not $SkipBuild) {
    Write-Host "Configuring..."
    cmake -S $SourceDir -B $BuildDir -G "MinGW Makefiles" -D CMAKE_PREFIX_PATH=$QtDir
    Write-Host "Building..."
    cmake --build $BuildDir --config Release
}

$exePath = Join-Path $BuildDir "simpleqt.exe"
if (-not (Test-Path $exePath)) {
    throw "Executable not found at $exePath. Build failed?"
}

if (Test-Path $OutputDir) {
    Remove-Item $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutputDir | Out-Null

# Deploy Qt runtime
$windeploy = Join-Path $QtDir "bin/windeployqt.exe"
if (-not (Test-Path $windeploy)) {
    throw "windeployqt not found at $windeploy"
}
& $windeploy --release --dir $OutputDir $exePath

# Copy app resources
Copy-Item (Join-Path $SourceDir "assets") $OutputDir -Recurse -Force

# Copy tools excluding virtual envs / runs
$toolsSource = Join-Path $SourceDir "tools"
$toolsTarget = Join-Path $OutputDir "tools"
if (-not (Test-Path $toolsTarget)) { New-Item -ItemType Directory -Path $toolsTarget | Out-Null }
robocopy $toolsSource $toolsTarget /E /XD ".venv" ".r-lib" "runs" | Out-Null
if ($LASTEXITCODE -gt 3) {
    throw "robocopy failed with exit code $LASTEXITCODE"
}

# Root files
Copy-Item $exePath $OutputDir -Force

if ($Zip) {
    $zipPath = "dist/ScriptToolbox.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path (Join-Path $OutputDir "*") -DestinationPath $zipPath
    Write-Host "Packaged to $zipPath"
} else {
    Write-Host "Packaged to $OutputDir"
}
