# Makes this PC the build machine for the Unreal edition (a GitHub self-hosted runner with the label "unreal"),
# running as a Windows service so it starts with the PC. Run it once, in PowerShell, from the repo folder:
#   powershell -ExecutionPolicy Bypass -File Tools\RegisterBuildMachine.ps1
# It asks for the one-time registration token from GitHub: the repo → Settings → Actions → Runners → New self-hosted
# runner → Windows; copy the token shown after --token. The token is only typed here, never saved in the repo.

$ErrorActionPreference = 'Stop'
$repo = 'https://github.com/DroneDirector/Airport-Dynasty-Pro-Unreal'
$dir = 'C:\actions-runner-adp'

New-Item -ItemType Directory -Force $dir | Out-Null
Set-Location $dir
if (-not (Test-Path "$dir\config.cmd")) {
    $version = (Invoke-RestMethod 'https://api.github.com/repos/actions/runner/releases/latest').tag_name.TrimStart('v')
    $zip = "actions-runner-win-x64-$version.zip"
    Write-Host "Downloading GitHub's runner $version…"
    Invoke-WebRequest "https://github.com/actions/runner/releases/download/v$version/$zip" -OutFile $zip
    Expand-Archive $zip -DestinationPath $dir -Force
    Remove-Item $zip
}
$token = Read-Host 'Paste the registration token from GitHub'
& .\config.cmd --unattended --url $repo --token $token --name "$env:COMPUTERNAME-unreal" --labels 'unreal' --work '_work' --runasservice --replace
Write-Host 'Done: the build machine runs as a service and starts with the PC.'
