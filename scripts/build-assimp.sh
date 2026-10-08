#!/bin/bash
# Builds libjassimp64.so (Assimp 5.4.3 + JNI part, with the Project Zomboid .X fix) for aarch64.
# usage: build-assimp.sh <workdir>      result: <workdir>/assimp/libjassimp64.so
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
W=$(realpath -m "${1:-build}"); mkdir -p "$W"; cd "$W"
JH=${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}
[ -d assimp-5.4.3 ] || { curl -fL --retry 3 -o assimp.tar.gz https://github.com/assimp/assimp/archive/refs/tags/v5.4.3.tar.gz
                         tar xzf assimp.tar.gz && rm assimp.tar.gz && patch -d assimp-5.4.3 -p1 < "$ROOT/src/assimp/0001-jassimp-and-xfile-fix.patch"
                         sed -i '/^          log$/d' assimp-5.4.3/CMakeLists.txt; }   # 'log' is an Android-only library
cd assimp-5.4.3 && rm -rf build && mkdir build && cd build
JAVA_HOME=$JH cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_INSTALL=OFF \
  -DASSIMP_NO_EXPORT=ON -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF -DASSIMP_BUILD_FBX_IMPORTER=ON \
  -DASSIMP_BUILD_GLTF_IMPORTER=ON -DASSIMP_BUILD_X_IMPORTER=ON -DBUILD_JASSIMP=ON \
  -DASSIMP_WARNINGS_AS_ERRORS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
JAVA_HOME=$JH make -j"$(nproc)" jassimp
mkdir -p "$W/assimp" && strip -o "$W/assimp/libjassimp64.so" libjassimp64.so
ls -l "$W/assimp/libjassimp64.so"
