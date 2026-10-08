# How it works

```
Steam ──> pz-arm64 (Python launcher, never touches the game dir)
            │  reads the game's own ProjectZomboid64.json, picks the B41 / B42 profile
            │  pzgen: scans the game's classes for `native` methods and the x86 libs' exports → binding tables (cached)
            ▼
        native arm64 JVM (Temurin 17/25) + LWJGL arm64 + Mesa
            │  System.loadLibrary("Lighting64") …  → finds libpzshim.so copy named like the x86 library
            ▼
        libpzshim.so  JNI_OnLoad reads <table>/<libname>.tbl, RegisterNatives with machine-code stubs
            │  stub → pzs_dispatch: rebuilds the argument list from the JNI descriptor
            ▼
        libzomdroidlinker.so  (pzb.c, wrapped_jni.c)    ← runs Box64 in library mode in-process
            │  Box64 (libbox64.so) executes the game's x86-64 libraries with a fake x86 JNIEnv (libjniwrapper.so)
            ▼
        game's x86-64 libs: Lighting64, PZPopMan64, PZPathFind64, PZBullet64, PZClipper64, fmodintegration64, ZNetJNI64 …
```

## Binding tables (`tool/pzgen.py`)
Pure Python, no compiler needed at install time. For every `lib*.so` in the game dir that exports `Java_*` functions,
the class files of the game are scanned for `ACC_NATIVE` methods, JNI names are mangled (short and long form) and
matched against the library's dynamic symbols. One table per library is written to `~/.cache/pz-arm64/<profile>-<key>/`.
The shim is copied under each library name (separate inodes – the JVM treats symlinks to one file as one library).
`libPZBullet64.so` (needs OpenGL, not available inside Box64) is served from `libPZBulletNoOpenGL64.so`; GL-only
functions become no-ops. `libjassimp64.so` is replaced by a real arm64 Assimp build.

## Things that were hard-won (don't rediscover)
* `LD_PRELOAD=<jre>/lib/libjsig.so` is mandatory: HotSpot uses SIGSEGV for safepoints, Box64 installs its own handler;
  signal chaining lets both coexist.
* Box64 defers library constructors until the main program starts; in library mode that never happens →
  `RunDeferredElfInit` after loading `libjniwrapper.so` (C++ static constructors).
* `context->fullpath` must be set (steamclient reads `/proc/self/exe`).
* **Every game library is loaded into its own symbol scope** (like `dlopen(RTLD_LOCAL)`). PZPathFind64 and PZBullet64
  both define a different class called `Chunk`; in one global scope PathFind calls Bullet's methods and crashes.
* Shim stubs must be `hidden` (PLT veneers clobber x16, which carries the stub index).
* Touch screens show up as GLFW joystick #0 and break the tutorial's pad indexing → `libpzhide.so` hides
  `/dev/input/eventN` via open/openat interposition.
* x86 FMOD `GetRecordNumDrivers` crashes → `-novoip`.
* x86 FMOD needs `libfmod.so.13` / `libfmodstudio.so.13` sonames (symlinks created under `~/.cache/pz-arm64/x86`).
* Strict Box64 float settings made things worse; the defaults in the launcher are the tested ones.
* `PZ_TRACE=1` (via `box64_env`) makes the shim log every forwarded JNI call with its first arguments, and a hex dump
  of direct buffers – invaluable for debugging.

## Credits
The idea of running Box64 as a library inside the JVM and the JNI wrapping layer come from
[Zomdroid](https://github.com/Not-a-dude/zomdroid) (MIT).
