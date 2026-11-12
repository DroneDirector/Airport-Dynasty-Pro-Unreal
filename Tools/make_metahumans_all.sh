#!/bin/bash
# Builds the MetaHuman people one at a time, each in its own hidden editor session (Epic's MetaHuman build doesn't
# take a second person in the same session: the shared parts it saved the first time trip it up). Already built
# people are kept. Usage: Tools/make_metahumans_all.sh [first] [last]   (default 0 119)
# A session that has finished its person but hangs on closing (seen 2026-11-12) is closed after a minute; one that
# runs past 25 minutes is stopped and the next person goes on.
R="$(cd "$(dirname "$0")/.." && pwd -W)"
ED="C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
LOG="$R/Saved/Logs/AirportDynastyPro.log"
FIRST=${1:-0}; LAST=${2:-119}
for i in $(seq $FIRST $LAST); do
  START=$(date +%s)
  "$ED" "$R/AirportDynastyPro.uproject" -ExecutePythonScript="$R/Tools/make_metahumans.py build 1 $i" -unattended -nosplash -RenderOffscreen -dx12 > /dev/null 2>&1 &
  PID=$!
  DONE_AT=0
  while kill -0 $PID 2>/dev/null; do
    sleep 10
    NOW=$(date +%s)
    if [ $DONE_AT -eq 0 ] && [ -f "$LOG" ] && [ $(stat -c %Y "$LOG") -ge $START ] && grep -qE "ADP_MH: MH_$(printf %03d $i)_|LogPython: Error" "$LOG"; then DONE_AT=$NOW; fi
    if { [ $DONE_AT -gt 0 ] && [ $((NOW - DONE_AT)) -gt 60 ]; } || [ $((NOW - START)) -gt 1500 ]; then
      WPID=$(cat /proc/$PID/winpid 2>/dev/null)
      [ -n "$WPID" ] && taskkill //F //T //PID $WPID > /dev/null 2>&1
      kill -9 $PID 2>/dev/null
      echo "(person $i: editor closed by force after $((NOW - START)) s)"
      break
    fi
  done
  wait $PID 2>/dev/null
  grep -E "ADP_MH: MH_|LogPython: Error" "$LOG" | tail -1 | sed 's/^.*ADP_MH: //'
done
echo "PEOPLE DONE"
