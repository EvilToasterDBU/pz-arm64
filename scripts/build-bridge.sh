#!/bin/bash
# Builds the arm64 bridge: libzomdroidlinker.so (Box64 init + JNI forwarders), libpzshim.so (universal JNI shim),
# libpzhide.so (LD_PRELOAD helper). Needs the Box64 tree from build-box64.sh and JNI headers (JAVA_HOME).
# usage: build-bridge.sh <workdir>      results in <workdir>/bridge/
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
W=$(realpath -m "${1:-build}")
S=$W/box64-src
JH=${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}
O=$W/bridge; mkdir -p "$O"
B=$ROOT/src/bridge
[ -f "$W/box64-src/build/libbox64.so" ] || { echo "build box64 first"; exit 1; }
cc -shared -fPIC -O2 -std=gnu11 -DARM64 -DCONFIG_64BIT -DDYNAREC -DNOGIT -DBUILD_DYNAMIC -funwind-tables \
  -I$S/src/include -I$S/src -I$S/src/wrapped/generated -I$S/src/dynarec/arm64 -I$JH/include -I$JH/include/linux -I$B \
  -Wl,-soname,libzomdroidlinker.so -Wl,--no-as-needed -o "$O/libzomdroidlinker.so" $B/pzb.c $B/wrapped_jni.c \
  -L$S/build -l:libbox64.so -lm -ldl -lpthread -Wl,-rpath,'$ORIGIN'
cc -shared -fPIC -O2 -std=gnu11 -I$B -I$JH/include -I$JH/include/linux \
  -Wl,-soname,libpzshim.so -Wl,-Bsymbolic -Wl,--no-as-needed -o "$O/libpzshim.so" $B/pzshim.c $B/stubs.S \
  -L"$O" -l:libzomdroidlinker.so -ldl
cc -shared -fPIC -O2 -nostdlib -o "$O/libpzhide.so" $B/pzhide.c
cp "$S/build/libbox64.stripped.so" "$O/libbox64.so"
ls -l "$O"
