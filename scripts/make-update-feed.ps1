#requires -Version 5.1
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [Parameter(Mandatory = $true)]
    [string]$ZipUrl,
    [string]$Notes = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Notes)) {
    $Notes = "Manual release $Version"
}

$payload = [ordered]@{
    version = $Version
    url     = $ZipUrl
    notes   = $Notes
} | ConvertTo-Json -Depth 3

if (-not (Test-Path "dist")) {
    New-Item -ItemType Directory -Path "dist" | Out-Null
}

$path = "dist/update.json"
$payload | Out-File -FilePath $path -Encoding utf8
Write-Host "Wrote update feed to $path"
Write-Host $payload
