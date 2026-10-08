#!/bin/bash
# pz-arm64 installer: puts the "Project Zomboid (arm64 native)" Steam compatibility tool into Steam's
# compatibilitytools.d and downloads the Java runtimes and LWJGL arm64 natives it needs.
#
#   ./install.sh                   from an unpacked release (or: curl -fsSL <raw url>/install.sh | bash)
#   ./install.sh --steam-dir DIR   non-standard Steam location
#   ./install.sh --no-b41          skip Build 41 runtime (Java 17 + LWJGL 3.2.3)
#   ./install.sh --no-b42          skip Build 42 runtime (Java 25 + LWJGL 3.4.1)
#   ./install.sh --version vX.Y.Z  specific release when run via curl (default: latest)
#
# Nothing of the game itself is touched or downloaded. Needs: aarch64 Linux, python3, curl, tar, sha256sum.
set -euo pipefail

REPO="${PZ_ARM64_REPO:-EvilToasterDBU/pz-arm64}"        # GitHub "owner/name" that publishes the release tarballs
STEAM_DIR=""; DO_B41=1; DO_B42=1; VERSION="latest"
while [ $# -gt 0 ]; do
  case "$1" in
    --steam-dir) STEAM_DIR="$2"; shift 2;;
    --no-b41) DO_B41=0; shift;;
    --no-b42) DO_B42=0; shift;;
    --version) VERSION="$2"; shift 2;;
    -h|--help) sed -n '2,12p' "$0"; exit 0;;
    *) echo "unknown option: $1" >&2; exit 2;;
  esac
done

say() { printf '\033[1m==> %s\033[0m\n' "$*"; }
die() { echo "error: $*" >&2; exit 1; }

[ "$(uname -m)" = aarch64 ] || die "this tool is for aarch64 machines (found $(uname -m))"
for c in python3 curl tar sha256sum; do command -v "$c" >/dev/null || die "missing: $c"; done

# ---- locate Steam
if [ -z "$STEAM_DIR" ]; then
  for c in "$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.steam/root" \
           "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam"; do
    [ -d "$c/steamapps" ] || [ -d "$c/config" ] && { STEAM_DIR=$(readlink -f "$c"); break; }
  done
fi
[ -n "$STEAM_DIR" ] && [ -d "$STEAM_DIR" ] || die "Steam directory not found, pass --steam-dir"
DEST="$STEAM_DIR/compatibilitytools.d/pz-arm64"
say "Steam: $STEAM_DIR"

# ---- get the tool files: next to this script (release / checkout with dist), or download the release
SRC=""
HERE=$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || true)
if [ -n "$HERE" ] && [ -f "$HERE/pz-arm64" ] && [ -d "$HERE/lib/bridge" ]; then SRC="$HERE"
elif [ -n "$HERE" ] && [ -f "$HERE/dist/pz-arm64/pz-arm64" ]; then SRC="$HERE/dist/pz-arm64"
else
  [ "$REPO" != "EvilToasterDBU/pz-arm64" ] || die "release source unknown (PZ_ARM64_REPO not set)"
  TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
  if [ "$VERSION" = latest ]; then URL_BASE="https://github.com/$REPO/releases/latest/download"; else URL_BASE="https://github.com/$REPO/releases/download/$VERSION"; fi
  say "downloading release ($VERSION)"
  curl -fL --retry 3 -o "$TMP/rel.tar.gz" "$URL_BASE/pz-arm64-linux-aarch64.tar.gz"
  (cd "$TMP" && curl -fsSL "$URL_BASE/pz-arm64-linux-aarch64.tar.gz.sha256" | sed 's|  .*|  rel.tar.gz|' | sha256sum -c -) || die "checksum mismatch"
  tar -C "$TMP" -xzf "$TMP/rel.tar.gz"; SRC="$TMP/pz-arm64"
fi

say "installing to $DEST"
mkdir -p "$DEST"
# keep downloaded runtimes and user options across updates
for f in pz-arm64 pzgen.py compatibilitytool.vdf toolmanifest.vdf VERSION; do cp -f "$SRC/$f" "$DEST/$f"; done
[ -f "$DEST/pz-arm64.json" ] || cp "$SRC/pz-arm64.json" "$DEST/pz-arm64.json"
rm -rf "$DEST/lib/bridge" "$DEST/lib/x86" "$DEST/lib/jassimp"; mkdir -p "$DEST/lib"
cp -r "$SRC/lib/bridge" "$SRC/lib/x86" "$SRC/lib/jassimp" "$DEST/lib/"
chmod +x "$DEST/pz-arm64"

# ---- Java runtimes (Eclipse Temurin, from the Adoptium API; checksum verified)
fetch_jre() {  # <major> <dir>
  [ -x "$DEST/$2/bin/java" ] && { say "Java $1 already present"; return; }
  say "downloading Temurin JRE $1 (aarch64)"
  local meta; meta=$(curl -fsSL "https://api.adoptium.net/v3/assets/latest/$1/hotspot?architecture=aarch64&image_type=jre&os=linux") \
    || die "cannot query Adoptium"
  local link sum
  read -r link sum < <(printf '%s' "$meta" | python3 -c 'import sys,json; p=json.load(sys.stdin)[0]["binary"]["package"]; print(p["link"], p["checksum"])')
  local t; t=$(mktemp); curl -fL --retry 3 -o "$t" "$link"
  echo "$sum  $t" | sha256sum -c - >/dev/null || { rm -f "$t"; die "checksum mismatch for Java $1"; }
  rm -rf "$DEST/$2"; mkdir -p "$DEST/$2"; tar -C "$DEST/$2" --strip-components=1 -xzf "$t"; rm -f "$t"
}
# ---- LWJGL arm64 natives (Maven Central; sha1 verified)
fetch_lwjgl() {  # <dir> <version> <modules...>
  local dir="$DEST/lib/$1/lwjgl-arm64" ver="$2"; shift 2
  mkdir -p "$dir"
  for m in "$@"; do
    local name=$([ "$m" = lwjgl ] && echo "lwjgl-$ver-natives-linux-arm64.jar" || echo "$m-$ver-natives-linux-arm64.jar")
    [ -f "$dir/$name" ] && continue
    local url="https://repo1.maven.org/maven2/org/lwjgl/$m/$ver/$name"
    say "downloading $name"
    curl -fL --retry 3 -o "$dir/$name" "$url"
    local want; want=$(curl -fsSL "$url.sha1"); want=${want%% *}
    [ "$(sha1sum "$dir/$name" | cut -d' ' -f1)" = "$want" ] || { rm -f "$dir/$name"; die "checksum mismatch for $name"; }
  done
}
if [ $DO_B41 = 1 ]; then fetch_jre 17 jre17; fetch_lwjgl b41 3.2.3 lwjgl lwjgl-glfw lwjgl-jemalloc lwjgl-opengl; fi
if [ $DO_B42 = 1 ]; then fetch_jre 25 jre25; fetch_lwjgl b42 3.4.1 lwjgl lwjgl-glfw lwjgl-jemalloc lwjgl-opengl lwjgl-stb; fi

cat <<MSG

Installed. Next steps:
  1. Restart Steam.
  2. Project Zomboid -> Properties -> Compatibility -> tick "Force the use of a specific Steam Play tool"
     and choose "Project Zomboid (arm64 native)".
  3. Launch the game. Logs: ~/Zomboid/pz-arm64.log and ~/Zomboid/projectzomboid.sh.log
Options (optional): $DEST/pz-arm64.json
MSG
