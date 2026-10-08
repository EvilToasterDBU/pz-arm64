#!/bin/bash
# Removes the pz-arm64 compatibility tool and its cache. The game and your saves (~/Zomboid) are not touched.
set -euo pipefail
STEAM_DIR="${1:-}"
if [ -z "$STEAM_DIR" ]; then
  for c in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"; do
    [ -d "$c/compatibilitytools.d/pz-arm64" ] && { STEAM_DIR=$(readlink -f "$c"); break; }
  done
fi
[ -n "$STEAM_DIR" ] || { echo "pz-arm64 is not installed (pass the Steam dir as argument if it is elsewhere)"; exit 0; }
echo "removing $STEAM_DIR/compatibilitytools.d/pz-arm64 and ~/.cache/pz-arm64"
rm -rf "$STEAM_DIR/compatibilitytools.d/pz-arm64" "$HOME/.cache/pz-arm64"
echo "done. Remember to switch the game's compatibility tool back in Steam."
