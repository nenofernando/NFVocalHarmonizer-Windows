param(
    [switch]$SkipBuild,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

if (-not $SkipBuild) {
    cmake --preset vst3-windows-x64
    cmake --build --preset build-vst3-windows-x64 --parallel
}

$buildRoot = Join-Path $PSScriptRoot "build-vst3-windows-x64"
$plugin = Join-Path $buildRoot "NFVocalHarmonizer_artefacts\Release\VST3\NF Vocal Harmonizer.vst3"
$outputDir = Join-Path $PSScriptRoot "dist"
$installerScript = Join-Path $PSScriptRoot "installer\windows\NFVocalHarmonizer.iss"

if (-not (Test-Path $plugin)) {
    throw "Windows VST3 was not generated: $plugin"
}

$isccCandidates = @(
    (Get-Command ISCC.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
) | Where-Object { $_ -and (Test-Path $_) }

if ($isccCandidates.Count -eq 0) {
    throw "Inno Setup 6 not found. Install it with: winget install JRSoftware.InnoSetup"
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
& $isccCandidates[0] "/DBuildRoot=$buildRoot" "/DOutputDir=$outputDir" $installerScript
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}

$installer = Join-Path $outputDir "NF-Vocal-Harmonizer-Windows-x64-Setup.exe"
if (-not (Test-Path $installer)) {
    throw "Installer was not generated: $installer"
}

$hash = Get-FileHash -Algorithm SHA256 $installer
Write-Host ""
Write-Host "Windows installer ready:"
Write-Host $installer
Write-Host "SHA256: $($hash.Hash)"
