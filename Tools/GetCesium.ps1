# Fetches Cesium for Unreal (Apache 2.0, by Cesium GS) into Plugins/CesiumForUnreal, the version built for this
# project's engine. It is kept out of git (about 1.3 GB); the PC release build runs this script when it is missing.
#   powershell -ExecutionPolicy Bypass -File Tools\GetCesium.ps1
$ErrorActionPreference = 'Stop'
$version = 'v2.31.0'
$engine = '57'
$root = Split-Path $PSScriptRoot -Parent
$dest = Join-Path $root 'Plugins'
if (Test-Path (Join-Path $dest 'CesiumForUnreal\CesiumForUnreal.uplugin')) { Write-Host 'Cesium for Unreal is already there.'; exit 0 }
New-Item -ItemType Directory -Force $dest | Out-Null
$zip = Join-Path $env:TEMP "CesiumForUnreal-$engine-$version.zip"
$url = "https://github.com/CesiumGS/cesium-unreal/releases/download/$version/CesiumForUnreal-$engine-$version.zip"
Write-Host "Downloading $url"
# curl retries and resumes, which a 1.3 GB download from GitHub can need
& curl.exe -L --fail --retry 8 --retry-delay 10 --retry-all-errors -C - -o $zip $url
if ($LASTEXITCODE -ne 0) { throw 'The download failed' }
Expand-Archive $zip -DestinationPath $dest -Force
Remove-Item $zip
Write-Host 'Cesium for Unreal is in Plugins\CesiumForUnreal.'
