#!/bin/bash
# The title film as a Bink video, part of the game (owner, 2026-11-19: the live-drawn title film stuttered; "it should
# be just a part of the game"): the game's own film material draws every frame (-UITitleRender, 30 a second, one music
# loop of 257.16 s) for a 16:9 screen (1920x1080) and a 32:9 one (3840x1080, the film frames itself to the screen),
# then Unreal's Bink 2 compressor makes Content/Movies/TitleFilm_16x9.bk2 and TitleFilm_32x9.bk2 at full quality.
# One game copy at a time; his editor may stay open.
#   bash Tools/make_title_bink.sh
R=/c/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal
UE="C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe"
BINK="C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/ThirdParty/Bink/Bink2ForUnreal.exe"
OUT=/f/ADP-TitleRender
mkdir -p $R/Content/Movies
for SIZE in "1920 1080 16x9" "3840 1080 32x9"; do
  set -- $SIZE; W=$1; H=$2; NAME=$3
  N=$(powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | Where-Object { \$_.Name -like 'AirportDynastyPro-Win64*' -or (\$_.Name -like 'UnrealEditor*' -and \$_.CommandLine -match ' -game') }).Count")
  [ "${N//[^0-9]/}" != "0" ] && { echo "STOP: another game copy is running"; exit 1; }
  rm -rf $OUT/$NAME; mkdir -p $OUT/$NAME
  echo "== frames $W x $H"
  timeout 2400 "$UE" "$(cd $R && pwd -W)/AirportDynastyPro.uproject" -game -UIShot "-UITitleRender=$W,$H,F:/ADP-TitleRender/$NAME" -windowed -ResX=1280 -ResY=720 > /dev/null 2>&1
  C=$(ls $OUT/$NAME | wc -l); echo "   $C frames"
  [ "$C" -ge 7700 ] || { echo "STOP: frames missing"; exit 1; }
  echo "== Bink $NAME"
  powershell -NoProfile -Command "\$p = Start-Process -FilePath '$BINK' -ArgumentList @('Bink2c','\"F:\\ADP-TitleRender\\$NAME\\t?????.jpg*1-$C\"','\"F:\\ADP-TitleRender\\TitleFilm_$NAME.bk2\"','/F30','/O','/#') -PassThru; \$p.WaitForExit(); \$p.ExitCode"
  cp $OUT/TitleFilm_$NAME.bk2 $R/Content/Movies/ && ls -la $R/Content/Movies/TitleFilm_$NAME.bk2 | cut -c25-
done
echo "== done"
