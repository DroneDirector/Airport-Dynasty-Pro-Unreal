#!/bin/bash
# THE AIRPLANE TEST (owner 2026-11-28), on his screen, full screen 3840 x 1080, visible (never -RenderOffscreen):
#   bash Tools/airplane_test.sh video    recorded: 60 fps ddagrab + h264_nvenc, the game's own sound (-RecordAudio), no pictures
#   bash Tools/airplane_test.sh photos   the same run (Tools/views/airplane-test.txt), a PNG at every shot4k, no recording
#   ... video|photos flight              only the part in the air: 10,000 ft and the landing (Tools/views/airplane-flight.txt)
# Out: F:/ADP-Export/airplane-test/ (the video, its -recorder.txt, three frames to look at) and a short summary there:
#   airplane-test-<mode>-summary.txt. His careers / saves and GameUserSettings are put aside and back (perf_views does the latter).
R="$(cd "$(dirname "$0")/.." && pwd)"; cd "$R"
M="${1:-}"; case "$M" in video|photos) ;; *) echo "use: bash Tools/airplane_test.sh video|photos [flight]"; exit 2 ;; esac
VF=Tools/views/airplane-test.txt; NM=airplane-test; [ "${2:-}" = flight ] && { VF=Tools/views/airplane-flight.txt; NM=airplane-flight; }
OUT=/f/ADP-Export/airplane-test; mkdir -p "$OUT"
[ -e /f/ADP-Export/gpu.lock ] && { echo "STOP: /f/ADP-Export/gpu.lock is taken (another run uses the graphics card)"; exit 1; }
tasklist | grep -qiE "UnrealEditor.exe|AirportDynastyPro" && { echo "STOP: the game or editor is running"; exit 1; }
echo "airplane-test $M $(date +%F_%T)" > /f/ADP-Export/gpu.lock
STAMP=$(date +%m%d-%H%M); SUMF="$OUT/airplane-test-$M-summary.txt"
KEEP=$(mktemp -d "$OUT/keep-XXXXXX")
for p in Careers Saves Autosaves; do [ -e "Saved/$p" ] && cp -rp "Saved/$p" "$KEEP/$p"; done
putback() { for p in Careers Saves Autosaves; do [ -e "$KEEP/$p" ] && { rm -rf "Saved/$p"; cp -rpT "$KEEP/$p" "Saved/$p"; }; done
  rm -rf "$KEEP"; rm -f /f/ADP-Export/gpu.lock; }
trap putback EXIT
rm -f Saved/Logs/Views.log
if [ "$M" = video ]; then
  VID="$OUT/$NM-$STAMP.mp4"
  python Tools/record_test.py "$(cygpath -w "$VID")" "$(cygpath -w "$R/Saved/Logs/Views.log")" 40 > "$OUT/recorder-console.txt" 2>&1 & RP=$!
  bash Tools/perf_views.sh "$VF" editor -AirplaneTest -RecordAudio -ViewsNoShots "-RecordAudioDir=F:/ADP-Export/airplane-test" > "$OUT/views-console.txt" 2>&1
  wait $RP; REC=$?
else
  T0=$(date +%s)
  bash Tools/perf_views.sh "$VF" editor -AirplaneTest > "$OUT/views-console.txt" 2>&1
fi
{ echo "AIRPLANE TEST ($M, $VF) $(date '+%F %H:%M')"
  grep -m1 "game exit code" Saved/Logs/Views-Summary.txt
  echo "missing subjects: $(grep -c 'subject missing' Saved/Logs/Views-Summary.txt)"; grep 'subject missing\|could not be made\|never spawned\|runway' Saved/Logs/Views-Summary.txt | head -8
  grep -i 'Traffic: test .* spawned' Saved/Logs/Views.log | sed 's/.*Traffic: /  /' | cut -c1-200 | head -4
  if [ "$M" = video ]; then echo "recorder exit $REC (0 = picture + sound in):"; cut -c1-220 "${VID%.mp4}-recorder.txt" 2>/dev/null | tail -8
  else echo "pictures: $(find Saved/Screenshots -name '*.png' -newermt "@$T0" 2>/dev/null | wc -l) new PNGs"; find Saved/Screenshots -name '*.png' -newermt "@$T0" 2>/dev/null | head -30; fi
} > "$SUMF"
cat "$SUMF"
