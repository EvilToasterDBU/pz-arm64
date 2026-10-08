# Building

Normally you do not need this – releases are built by GitHub Actions (`.github/workflows/build.yml`).

## Requirements
* **aarch64 build host** (or arm64 container) with: `gcc g++ cmake make patch curl python3` and zlib headers (`zlib1g-dev` / `zlib-devel`) and a JDK (for `jni.h`)
* **x86-64 host** for the x86 half (any distro with `g++` and a JDK); Ubuntu 22.04 is used in CI so the bundled
  x86 libstdc++ is old enough for Box64's libc wrappers

## Steps
```bash
# 1. on any x86-64 machine
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 scripts/build-x86-wrapper.sh build/x86
# 2. on aarch64 (copy build/x86 over)
JAVA_HOME=/usr/lib/jvm/java-17-openjdk-arm64 X86_DIR=$PWD/build/x86 scripts/build.sh
# result: dist/pz-arm64/  and  dist/pz-arm64-<version>-linux-aarch64.tar.gz
./install.sh               # installs from dist/ (downloads JREs and LWJGL natives)
```
Individual pieces: `scripts/build-box64.sh`, `scripts/build-bridge.sh`, `scripts/build-assimp.sh`.

Verified: build-box64.sh, build-bridge.sh and build-assimp.sh run to completion in a Fedora aarch64 container.
Build old-glibc-friendly binaries by building inside `ubuntu:22.04` (CI does this natively on `ubuntu-22.04-arm`).

## Tests
`tests/*.java` are isolated JNI smoke tests (lighting, FMOD, Steam, server browser) that load one x86 library through
the bridge without starting the whole game; they expect a game install and are meant for development.
