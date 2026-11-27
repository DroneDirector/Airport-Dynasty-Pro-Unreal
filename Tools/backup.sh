#!/usr/bin/env bash
# Back up files or folders before they are overwritten (owner 2026-11-27: "always have a backup of what you're working on").
# Usage: bash Tools/backup.sh <path> [path...]   -> F:/ADP-Backups/<yyyy-mm-dd_hhmmss>/<same relative path>
# Every builder runs this on the files it will write (glb, blend, textures, json) BEFORE its first write.
set -e
# (backups older than 14 days are removed: F: is nearly full)
find F:/ADP-Backups -mindepth 1 -maxdepth 1 -type d -mtime +14 -exec rm -rf {} + 2>/dev/null || true
cd "$(dirname "$0")/.."
Dest="F:/ADP-Backups/$(date +%Y-%m-%d_%H%M%S)"
for P in "$@"; do
  [ -e "$P" ] || { echo "backup: $P not found, skipped"; continue; }
  mkdir -p "$Dest/$(dirname "$P")"
  cp -a "$P" "$Dest/$P"
  echo "backup: $P -> $Dest/$P"
done
