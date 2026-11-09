# Downloads the rest of the raw data for the PC version's scenery, once, outside the repo (default F:\ADP-Terrain):
#  - Iceland: ÍslandsDEM v1.0, 10 m (Landmælingar Íslands / National Land Survey of Iceland, CC BY 4.0), from ftp.lmi.is.
#  - The land cover: ESA WorldCover 10 m 2021 v200 (CC BY 4.0), its 2,651 map tiles from the public AWS bucket
#    esa-worldcover (no account needed). Credit: "© ESA WorldCover project / Contains modified Copernicus Sentinel data
#    (2021) processed by ESA WorldCover consortium".
# Resumes where it stopped when run again.
#   powershell -ExecutionPolicy Bypass -File Tools\GetLandData.ps1 [-Dest F:\ADP-Terrain]
param([string]$Dest = 'F:\ADP-Terrain')
$ErrorActionPreference = 'Stop'

# Iceland
$ice = Join-Path $Dest 'iceland'
New-Item -ItemType Directory -Force $ice | Out-Null
$iceFile = Join-Path $ice 'IslandsDEMv1.0_10x10m_isn93_zmasl.tif'
if (-not (Test-Path $iceFile)) {
    & curl.exe -s -L --fail --retry 8 --retry-delay 10 -C - -o $iceFile 'https://ftp.lmi.is/gisdata/raster/IslandsDEMv1.0_10x10m_isn93_zmasl.tif'
    Write-Host 'Iceland downloaded.'
}

# ESA WorldCover
$bucket = 'https://esa-worldcover.s3.eu-central-1.amazonaws.com'
$wc = Join-Path $Dest 'worldcover'
New-Item -ItemType Directory -Force $wc | Out-Null
$keys = New-Object System.Collections.Generic.List[string]
$token = $null
do {
    $u = "$bucket/?list-type=2&prefix=v200/2021/map/&max-keys=1000" + ($(if ($token) { '&continuation-token=' + [uri]::EscapeDataString($token) } else { '' }))
    [xml]$x = (Invoke-WebRequest $u -UseBasicParsing).Content
    foreach ($c in $x.ListBucketResult.Contents) { if ($c.Key -like '*_Map.tif') { $keys.Add($c.Key) } }
    $token = $x.ListBucketResult.NextContinuationToken
} while ($x.ListBucketResult.IsTruncated -eq 'true')
Write-Host "$($keys.Count) WorldCover tiles"
$i = 0
foreach ($k in $keys) {
    $i++
    $file = Join-Path $wc (Split-Path $k -Leaf)
    if (Test-Path $file) { continue }
    & curl.exe -s -L --fail --retry 5 --retry-delay 5 -o $file "$bucket/$k"
    if ($i % 100 -eq 0) { Write-Host ("WorldCover {0}% ({1} of {2})" -f [math]::Round($i * 100 / $keys.Count), $i, $keys.Count) }
}
Write-Host 'WorldCover downloaded.'
