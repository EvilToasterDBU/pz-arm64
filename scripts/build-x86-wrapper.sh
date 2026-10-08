#!/bin/bash
# Builds libjniwrapper.so (x86-64): the fake JNIEnv / JavaVM function tables that the game's x86 JNI libraries see.
# Every table slot points at a zomdroid_jni_* function that lives in the arm64 bridge (libzomdroidlinker.so),
# so this library only needs to record the dependency; a stub library with the same symbols is linked against.
# Needs an x86-64 gcc/g++ (cross or native) and JNI headers (JAVA_HOME or a bundled jni.h directory).
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=${1:-build/x86}
CXX=${CXX:-g++}
JH=${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac || command -v java)")")")}
mkdir -p "$OUT"
# stub with every symbol the wrapper references
grep -o 'EXTERN([A-Za-z0-9_]*' src/x86/jniwrapper.cpp | sed 's/EXTERN(//' | sort -u | awk '{printf "void *zomdroid_jni_%s;\n",$1}' > "$OUT/stub.c"
$CXX -x c -shared -fPIC -o "$OUT/libzomdroidlinker.so" -Wl,-soname,libzomdroidlinker.so "$OUT/stub.c"
$CXX -shared -fPIC -O2 -std=gnu++17 -I"$JH/include" -I"$JH/include/linux" \
  -Wl,-soname,libjniwrapper.so -Wl,--no-as-needed \
  -o "$OUT/libjniwrapper.so" src/x86/jniwrapper.cpp -L"$OUT" -l:libzomdroidlinker.so
rm -f "$OUT/libzomdroidlinker.so" "$OUT/stub.c"
echo "built $OUT/libjniwrapper.so"
# the x86-64 C++ runtime the game's libraries run against inside Box64 (taken from this machine)
for l in libstdc++.so.6 libgcc_s.so.1; do
  p=$($CXX -print-file-name=$l); cp -L "$p" "$OUT/$l"
done
ls -l "$OUT"
