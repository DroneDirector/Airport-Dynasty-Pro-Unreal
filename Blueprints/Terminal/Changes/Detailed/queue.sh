#!/bin/bash
# The 4K pictures one after the other, each taking its own turn on the GPU (one.sh). To stop after the picture in hand:
#   touch F:/ADP-Export/tc_stop          (also ends by itself if another queue.sh holds the lock)
cd "$(dirname "$0")" || exit 1
mkdir F:/ADP-Export/tc_queue.lock 2>/dev/null || { echo "another queue runs"; exit 1; }
trap 'rmdir F:/ADP-Export/tc_queue.lock' EXIT
rm -f F:/ADP-Export/tc_stop
for id in "$@"; do
  [ -e F:/ADP-Export/tc_stop ] && { echo STOPPED; exit 0; }
  bash one.sh "$id"
done
echo ALLDONE
