# The terrain packs (Tools/PackTerrain.py) published as their own GitHub release (terrain-v<N>), which the game's
# terrain downloader reads (TerrainPacks.cpp). Uploads pack by pack, printing a line every 500 MB.
#   powershell -File Tools/UploadTerrain.ps1 [-Packs F:\ADP-Terrain\packs-v2] [-Tag terrain-v1]
#   -Only 'landcover-*.zip' uploads just those packs (with the manifest)
param([string]$Packs = 'F:\ADP-Terrain\packs-v2', [string]$Tag = 'terrain-v1', [string]$Only = 'terrain-*.zip')
$ErrorActionPreference = 'Stop'
$env:Path = [System.Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [System.Environment]::GetEnvironmentVariable('Path', 'User')
Set-Location (Join-Path $PSScriptRoot '..')
$notes = @'
The world's terrain for Airport Dynasty Pro (Unreal), PC: downloaded by the game itself (Settings > The world's terrain).
Copernicus DEM GLO-90 worldwide, the Faroe Islands' national 10 m model (Umhvorvisstovan) and Iceland's IslandsDEM 10 m
(LMI, CC BY 4.0) on top; the sea marked for real water. 129 region packs, one manifest with their checksums.
Credit: Copernicus DEM GLO-90 (c) DLR e.V. 2010-2014 and (c) Airbus Defence and Space GmbH 2014-2018 provided under
COPERNICUS by the European Union and ESA; Data from the Faroese Environment Agency, Umhvorvisstovan; IslandsDEM (c) Landmaelingar Islands.
'@
$have = $false
try { gh release view $Tag *> $null; $have = ($LASTEXITCODE -eq 0) } catch { $have = $false }
if (-not $have) { gh release create $Tag --prerelease --target main --title "World terrain ($Tag)" --notes $notes "$Packs\manifest.json" | Out-Null }
else { gh release upload $Tag "$Packs\manifest.json" --clobber | Out-Null }
$files = Get-ChildItem $Packs -Filter $Only | Sort-Object Name
$sent = 0.0; $next = 500MB
foreach ($f in $files) {
  gh release upload $Tag $f.FullName --clobber | Out-Null
  if ($LASTEXITCODE -ne 0) { "FAILED $($f.Name)"; exit 1 }
  $sent += $f.Length
  while ($sent -ge $next) { '{0:N1} GB uploaded  file {1}' -f ($sent / 1e9), $f.Name; $next += 500MB }
}
'done: {0} packs, {1:N2} GB' -f $files.Count, ($sent / 1e9)
