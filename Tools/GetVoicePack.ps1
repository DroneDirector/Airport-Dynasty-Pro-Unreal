# The announcer's voice pack (Kokoro-82M, Apache 2.0; the quantised ONNX build from kokoro-onnx, MIT; misaki's British
# dictionary, Apache 2.0) into Content/Voice, where the game reads it and the release ships it. Run once per checkout.
$ErrorActionPreference = 'Stop'
$D = Join-Path $PSScriptRoot '..\Content\Voice'
New-Item -ItemType Directory -Force $D | Out-Null
$B = 'https://github.com/DroneDirector/Microsoft-Flight-Simulator-Projects/releases/download/amp-voice-pack-2'
foreach ($f in 'pack.json', 'NOTICE.txt', 'LICENSE-Kokoro-Apache-2.0.txt', 'LICENSE-kokoro-onnx-MIT.txt', 'LICENSE-misaki-Apache-2.0.txt', 'lexicon-gb.json.gz', 'voice-bf_emma.bin') {
  $o = Join-Path $D $f
  if (-not (Test-Path $o)) { Write-Host "Getting $f"; Invoke-WebRequest "$B/$f" -OutFile $o -UseBasicParsing }
}
# the full-precision model (kokoro-onnx's release, MIT; the model Apache 2.0): several times faster than the 8-bit one
# on a PC's processor
$o = Join-Path $D 'kokoro-v1.0.onnx'
if (-not (Test-Path $o)) { Write-Host 'Getting kokoro-v1.0.onnx'; Invoke-WebRequest 'https://github.com/thewh1teagle/kokoro-onnx/releases/download/model-files-v1.0/kokoro-v1.0.onnx' -OutFile $o -UseBasicParsing }
