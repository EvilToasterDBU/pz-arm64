#!/bin/bash
# Builds libbox64.so: Box64 as a shared library (library mode, glibc, aarch64), from the Zomdroid fork.
# usage: build-box64.sh <workdir>      result: <workdir>/box64-src/build/libbox64.so
set -euo pipefail
W=$(realpath -m "${1:-build}")
SHA=3578258e2c240fa0dc52d23a6631687eaa709eef            # Not-a-dude/zomdroid-box64, branch "zomdroid"
mkdir -p "$W"; cd "$W"
if [ ! -d box64-src ]; then
  curl -fL --retry 3 -o box64.tar.gz "https://github.com/Not-a-dude/zomdroid-box64/archive/$SHA.tar.gz"
  mkdir box64-src && tar xzf box64.tar.gz --strip-components=1 -C box64-src && rm box64.tar.gz
fi
cd box64-src
# --- adaptations for library mode on plain glibc (each is idempotent)
sed -i 's/^#ifndef ANDROID$/#if !defined(ANDROID) \&\& !defined(BUILD_DYNAMIC)/' src/mallochook.c     # no malloc hooks inside the JVM
sed -i 's/^#if defined(ANDROID) || defined(STATICBUILD)$/#if defined(ANDROID) || defined(STATICBUILD) || defined(BUILD_DYNAMIC)/' src/include/debug.h
sed -i 's|^#include "bits/timespec.h"$|#include <time.h>|' src/include/myalign.h                      # private glibc header
grep -q BUILD_DYNAMIC src/mallochook.c && grep -q BUILD_DYNAMIC src/include/debug.h && grep -q '<time.h>' src/include/myalign.h \
  || { echo "box64 patches did not apply (upstream changed?)"; exit 1; }
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DARM_DYNAREC=ON -DNOGIT=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_C_FLAGS="-DBUILD_DYNAMIC -fPIC" -DCMAKE_CXX_FLAGS="-DBUILD_DYNAMIC -fPIC" -DCMAKE_ASM_FLAGS="-fPIC"
make -j"$(nproc)" box64
strip -o libbox64.stripped.so libbox64.so
ls -l libbox64.so libbox64.stripped.so
