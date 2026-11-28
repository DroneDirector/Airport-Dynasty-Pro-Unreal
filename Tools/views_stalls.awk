# The stall watch's entries in ADP-Perf.log (PerfLog.cpp, -StallWatch), short, for Views-Summary.txt (perf_views.sh):
# each stall once at its first look and once at its longest, with the Views step it was in, where the game thread was
# and up to three of the game's own places on its call stack.
function close_entry() { if (ins) { if (g1 == "") g1 = ent; gl = ent; ins = 0 } }
function flush() { if (g1 != "") { ++stalls; print g1; if (gl != g1) print gl } g1 = ""; gl = "" }
{ sub(/\r$/, "") }
/STALL/ {
  close_entry(); ms = $0; sub(/.*moved for /, "", ms); sub(/ ms.*/, "", ms); ms += 0
  if (ms <= pm) flush(); pm = ms
  ent = $0; n = 0; g = 0; ins = 1; next
}
ins && /^    / { n++; if (n == 1 || ($0 ~ /AirportDynastyPro/ && g < 3)) { ent = ent "\n" $0; if ($0 ~ /AirportDynastyPro/) g++ } next }
{ close_entry() }
END { close_entry(); flush(); print stalls + 0 " stalls" }
