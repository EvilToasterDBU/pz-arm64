#!/bin/bash
# Builds everything for aarch64 and assembles dist/pz-arm64/ (+ a release tarball).
# Run on an aarch64 Linux machine (or in an arm64 container) with: gcc g++ cmake make patch curl python3 and a JDK (JNI headers).
# The x86-64 part (libjniwrapper.so and the x86 C++ runtime) comes from build-x86-wrapper.sh on any x86-64 machine,
# pass its output dir with X86_DIR=...  (CI does this automatically).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
W=${BUILD_DIR:-$ROOT/build}
D=$ROOT/dist/pz-arm64
VERSION=${VERSION:-$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)}
[ "$(uname -m)" = aarch64 ] || { echo "this script must run on aarch64"; exit 1; }
[ -n "${X86_DIR:-}" ] && [ -f "$X86_DIR/libjniwrapper.so" ] || { echo "set X86_DIR to the output of scripts/build-x86-wrapper.sh (needs libjniwrapper.so, libstdc++.so.6, libgcc_s.so.1)"; exit 1; }

"$ROOT/scripts/build-box64.sh" "$W"
"$ROOT/scripts/build-bridge.sh" "$W"
"$ROOT/scripts/build-assimp.sh" "$W"

rm -rf "$D"; mkdir -p "$D/lib/bridge" "$D/lib/x86" "$D/lib/jassimp"
cp "$ROOT"/tool/{pz-arm64,pzgen.py,compatibilitytool.vdf,toolmanifest.vdf,pz-arm64.json} "$D/"
cp "$W"/bridge/*.so "$D/lib/bridge/"
cp "$X86_DIR"/{libjniwrapper.so,libstdc++.so.6,libgcc_s.so.1} "$D/lib/x86/"
cp "$W/assimp/libjassimp64.so" "$D/lib/jassimp/"
cp "$ROOT/install.sh" "$ROOT/uninstall.sh" "$ROOT/LICENSE" "$ROOT/NOTICE" "$D/"
echo "$VERSION" > "$D/VERSION"
chmod +x "$D/pz-arm64"
tar -C "$ROOT/dist" -czf "$ROOT/dist/pz-arm64-$VERSION-linux-aarch64.tar.gz" pz-arm64
ls -l "$ROOT/dist"
