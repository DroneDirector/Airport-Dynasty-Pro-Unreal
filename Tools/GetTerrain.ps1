# Downloads the raw elevation data for the PC version's terrain, once, outside the repo (default F:\ADP-Terrain):
# Copernicus DEM GLO-90 for the whole world, from its public AWS open-data bucket (no account needed). The Faroese
# national model and ÍslandsDEM are fetched by hand from Føroyakort and LMÍ (their terms are kept with them).
# Copernicus credit: "© DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by
# the European Union and ESA; all rights reserved." Resumes where it stopped when run again.
#   powershell -ExecutionPolicy Bypass -File Tools\GetTerrain.ps1 [-Dest F:\ADP-Terrain]
param([string]$Dest = 'F:\ADP-Terrain')
$ErrorActionPreference = 'Stop'
$bucket = 'https://copernicus-dem-90m.s3.amazonaws.com'
$out = Join-Path $Dest 'glo90'
New-Item -ItemType Directory -Force $out | Out-Null

# every tile's file in the bucket (the listing comes in pages of 1,000)
$keys = New-Object System.Collections.Generic.List[string]
$token = $null
do {
    $u = "$bucket/?list-type=2&max-keys=1000" + ($(if ($token) { '&continuation-token=' + [uri]::EscapeDataString($token) } else { '' }))
    [xml]$x = (Invoke-WebRequest $u -UseBasicParsing).Content
    foreach ($c in $x.ListBucketResult.Contents) { if ($c.Key -like '*_DEM.tif') { $keys.Add($c.Key) } }
    $token = $x.ListBucketResult.NextContinuationToken
} while ($x.ListBucketResult.IsTruncated -eq 'true')
Write-Host "$($keys.Count) tiles"

$i = 0
foreach ($k in $keys) {
    $i++
    $file = Join-Path $out (Split-Path $k -Leaf)
    if (Test-Path $file) { continue }
    & curl.exe -s -L --fail --retry 5 --retry-delay 5 -o $file "$bucket/$k"
    if ($i % 500 -eq 0) { Write-Host ("{0}% ({1} of {2})" -f [math]::Round($i * 100 / $keys.Count), $i, $keys.Count) }
}
Write-Host 'GLO-90 downloaded.'
