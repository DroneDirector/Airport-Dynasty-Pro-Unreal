# Fetches AMD's FSR plugin for Unreal Engine (v4.1.1: FSR upscaling and frame generation; the plugin under the MIT
# licence, AMD's FidelityFX libraries under AMD's free binary redistribution licence) into Plugins/FSR, the build for
# this project's engine (5.7). Kept out of git (about 1 GB); the PC release build runs this when it is missing.
#   powershell -ExecutionPolicy Bypass -File Tools\GetFSR.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dest = Join-Path $root 'Plugins\FSR'
if (Test-Path (Join-Path $dest 'FSR.uplugin')) { Write-Host 'The FSR plugin is already there.'; exit 0 }
$zip = Join-Path $env:TEMP 'UE-FSR-4_1_1.zip'
& curl.exe -L --fail --retry 8 --retry-delay 10 --retry-all-errors -C - -o $zip 'https://gpuopen.com/download/UE-FSR-4_1_1.zip'
if ($LASTEXITCODE -ne 0) { throw 'The download failed' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [System.IO.Compression.ZipFile]::OpenRead($zip)
$pre = 'UE-FSR-4_1_1/FSR-570/FSR/'
foreach ($e in $z.Entries) {
    if (-not $e.FullName.StartsWith($pre) -or $e.FullName.EndsWith('/')) { continue }
    $out = Join-Path $dest ($e.FullName.Substring($pre.Length))
    New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $out, $true)
}
$lic = $z.GetEntry('UE-FSR-4_1_1/FSR-570/LICENSE.txt'); [System.IO.Compression.ZipFileExtensions]::ExtractToFile($lic, (Join-Path $dest 'LICENSE.txt'), $true)
$z.Dispose(); Remove-Item $zip
Write-Host 'The FSR plugin is in Plugins\FSR.'
