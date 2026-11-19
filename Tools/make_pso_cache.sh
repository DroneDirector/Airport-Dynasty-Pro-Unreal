#!/bin/bash
# The shipped pipeline cache (to-do 75, owner 2026-11-19: "create the shader cache feature, which should improve
# performance at night"): the first time the game draws something new - a lamp at night, rain, a fire truck's lights -
# the graphics card has to build a pipeline for it, which is a stall. Recorded once and shipped, those pipelines are
# built in the background from the start instead (r.ShaderPipelineCache.*, Config/DefaultEngine.ini).
#  1. a plain package of the current game (shared shader library with stable keys: bShareMaterialShaderCode)
#  2. the stress walk in it with -logPSO (day and night, the jetways, the apron, the tax free ...): every pipeline the
#     game binds is written down (*.rec.upipelinecache)
#  3. Unreal's ShaderPipelineCacheTools turns the recording + the cook's stable shader keys (*.shk) into the cache
#     Build/Windows/PipelineCaches/AirportDynastyPro_PCD3D_SM6.spc, which every later package ships.
# One game copy at a time; nothing is published.
#   bash Tools/make_pso_cache.sh
R=/c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal
OUT=/f/ADP-PSOBuild
UE=/c/Program\ Files/Epic\ Games/UE_5.7
cd $R
N=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -like 'AirportDynastyPro-Win64*' -or (\$_.Name -like 'UnrealEditor*' -and \$_.CommandLine -match ' -game') }).Count")
[ "${N//[^0-9]/}" != "0" ] && { echo "STOP: another game copy is running"; exit 1; }
echo "== 1. a plain package for the recording"
rm -rf Content/Web && cp -r Airline-Manager-Pro-Babylon/www Content/Web && rm -rf Content/Data && cp -r Data Content/Data
rm -rf $OUT; mkdir -p $OUT; touch $OUT/GUS-none
"$UE/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="$(pwd -W)/AirportDynastyPro.uproject" -noP4 -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive -archivedirectory="F:\\ADP-PSOBuild" -utf8output -unattended -IgnoreCookErrors > $OUT/uat.log 2>&1
grep -q "BUILD SUCCESSFUL" $OUT/uat.log || { tail -20 $OUT/uat.log; echo "STOP: the package failed"; exit 1; }
SHK=$(find -L /f/UnrealCache/ADP-Saved/Cooked/Windows -name "*PCD3D_SM6.shk" -newer $OUT/GUS-none 2>/dev/null | head -20)
echo "   stable shader keys: $(echo "$SHK" | grep -c shk) files"
[ -n "$SHK" ] || { echo "STOP: no .shk files from the cook (bShareMaterialShaderCode)"; exit 1; }
echo "== 2. the stress walk with -logPSO (no time limit - the owner ends a walk)"
L="$LOCALAPPDATA/AirportDynastyPro/Saved"
rm -rf "$L/CollectedPSOs"
G="$L/Config/Windows/GameUserSettings.ini"; cp "$G" $OUT/GUS-owner.ini
EXE=$(ls -d $OUT/Windows/AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe 2>/dev/null || ls -d $OUT/*/AirportDynastyPro/Binaries/Win64/AirportDynastyPro-Win64-Shipping.exe | head -1)
(cd "$(dirname "$EXE")" && ADP_LOG=1 ./AirportDynastyPro-Win64-Shipping.exe -UIShot -UIPerfWalk -logPSO -fullscreen > /dev/null 2>&1)
cp $OUT/GUS-owner.ini "$G"
REC=$(find "$L/CollectedPSOs" "$(dirname "$EXE")/../../Saved/CollectedPSOs" -name "*.rec.upipelinecache" 2>/dev/null)
echo "   recordings: $(echo "$REC" | grep -c upipelinecache)"
[ -n "$REC" ] || { echo "STOP: no recording (-logPSO)"; exit 1; }
echo "== 3. the cache"
mkdir -p Build/Windows/PipelineCaches
ARGS=""; for f in $REC $SHK; do ARGS="$ARGS \"$(cygpath -w "$f")\""; done
eval "\"$UE/Engine/Binaries/Win64/UnrealEditor-Cmd.exe\" \"$(pwd -W)/AirportDynastyPro.uproject\" -run=ShaderPipelineCacheTools expand $ARGS \"$(pwd -W)/Build/Windows/PipelineCaches/AirportDynastyPro_PCD3D_SM6.spc\" -unattended -nullrhi" > $OUT/expand.log 2>&1
ls -la Build/Windows/PipelineCaches/ | tail -3
grep -a -i "pipeline\|PSO" $OUT/expand.log | tail -5
echo "== done: the next package ships the cache"
