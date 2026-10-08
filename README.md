# pz-arm64

**Run Project Zomboid (Build 41 and Build 42) natively on aarch64 Linux handhelds and SBCs** – a Steam compatibility
tool that runs the game on a *native arm64 Java VM* and only emulates the few x86-64 native libraries the game ships.

[Русская версия](README.ru.md)

## Why not just emulate everything?
Project Zomboid is Java, but its Linux depot only contains x86-64 native libraries (lighting, zombie population,
pathfinding, physics, FMOD sound, Steam networking, …). Running the *whole* game under an x86 emulator
(FEX / Box64) works but is slow and fragile. pz-arm64 instead:

* runs the game's Java code on a **native arm64 JVM** (Temurin 17 for B41, 25 for B42) with **LWJGL arm64** natives and
  the system's **Mesa** OpenGL driver;
* loads the game's **x86-64 `.so` libraries inside the JVM process with [Box64](https://github.com/ptitSeb/box64) in
  library mode**; a small shim forwards every JNI call into the emulated code (idea and JNI layer from
  [Zomdroid](https://github.com/Not-a-dude/zomdroid));
* builds all bindings **at launch time from the game's own files** – **the game directory is never modified**, so
  Steam updates and "verify files" keep working.

Working: menu, single player, lighting, zombies, vehicles, physics, FMOD sound, Steam (workshop, server browser) and
multiplayer – at good frame rates, on both Build 41 and Build 42.

**Tested on:** Retroid Pocket 6 (Snapdragon 8 Gen 2, Adreno 740, Mesa freedreno) running
[Armada OS](https://github.com/armada-os/armada) (a SteamOS-like distribution for ARM handhelds). Other aarch64
devices should work in principle but have not been tested.

## Requirements
* aarch64 Linux with Steam installed and a working OpenGL driver (Mesa / freedreno, panfrost …) and a display server
* Project Zomboid installed through Steam (Linux depot; use the default Linux build, not a Windows/Proton one)
* `python3`, `curl`, `tar`, `sha256sum` for the installer; glibc ≥ 2.35
* ~400 MB of disk space, ~3 GB RAM free for the game

## Install
**Preferred way: download the ready-made release** from the
[GitHub Releases page](https://github.com/EvilToasterDBU/pz-arm64/releases) (`pz-arm64-linux-aarch64.tar.gz`), unpack it and run the
installer:
```bash
tar xzf pz-arm64-linux-aarch64.tar.gz && cd pz-arm64 && ./install.sh
```
Options: `--steam-dir DIR`, `--no-b41`, `--no-b42`. One-line alternative (downloads the latest release for you):
```bash
curl -fsSL https://github.com/EvilToasterDBU/pz-arm64/releases/latest/download/install.sh | bash
```
Building from source is possible but not needed for normal use (see below).

Then restart Steam and, in *Project Zomboid → Properties → Compatibility*, force **Project Zomboid (arm64 native)**.
The installer downloads Eclipse Temurin JREs and LWJGL arm64 natives from their official sources (checksums verified);
no game files are downloaded or redistributed.

Uninstall: `./uninstall.sh`.

## Options
`compatibilitytools.d/pz-arm64/pz-arm64.json` (all optional):

| key | default | meaning |
|---|---|---|
| `sound` | `true` | real FMOD sound through the bridge (`false` = `-nosound`) |
| `steam` | `true` | real Steam API / networking (`false` = offline, `-nosteam`) |
| `voice` | `false` | in-game voice chat (currently crashes the emulated FMOD) |
| `xmx` | game default | e.g. `"4096m"` |
| `jvm_flags`, `game_args` | `[]` | extra flags |
| `box64_env` | see source | environment passed to Box64 (`BOX64_*`) |
| `hide_touchscreens` | `true` | hide touch screens from GLFW (they otherwise appear as gamepad #0) |

Logs: `~/Zomboid/pz-arm64.log` (launcher) and `~/Zomboid/projectzomboid.sh.log` (game + bridge).

## Build from source
See [docs/BUILDING.md](docs/BUILDING.md). How it works: [docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md).
Problems: [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

## Status / limitations
* Tested on one device only (Retroid Pocket 6 / Armada OS); reports for other SoCs / GPUs / distributions are very welcome.
* Build 41: touch-screen taps were not registered on the first-run Terms of Service screen (the pointer moved, the click did not); a gamepad / Steam Input virtual mouse works. Touch works in Build 42 menus.
* Voice chat is disabled. Newer game builds may need new bindings or fixes – the binding tables are generated
  automatically, but an update can still introduce incompatibilities.
* Not affiliated with The Indie Stone, Valve, Firelight (FMOD), Zomdroid or Box64.

## Credits
This project stands on the work of others:

* **[Zomdroid](https://github.com/Not-a-dude/zomdroid)** (liamelui, Not-a-dude and contributors) – the idea of running
  Box64 as a library inside the JVM and the JNI wrapping layer this project's bridge is derived from (MIT).
* **[Box64](https://github.com/ptitSeb/box64)** by ptitSeb and contributors – the x86-64 emulator that executes the game's
  native libraries; used via the [zomdroid-box64](https://github.com/Not-a-dude/zomdroid-box64) fork (MIT).
* **[Assimp](https://github.com/assimp/assimp)** – model loading (`libjassimp64.so` is built from it, BSD-3).
* **[LWJGL](https://www.lwjgl.org)** – the arm64 native bindings (OpenGL, GLFW, stb, jemalloc) (BSD-3).
* **[Eclipse Temurin](https://adoptium.net)** (Adoptium) – the arm64 Java runtimes (GPLv2 + Classpath Exception).
* **[Mesa](https://www.mesa3d.org)** – the OpenGL driver (freedreno) the game renders with.
* **[Armada OS](https://github.com/armada-os/armada)** – the platform this was developed and tested on.
* **The Indie Stone** – Project Zomboid itself. You need your own copy of the game.

Full licence information: [NOTICE](NOTICE) and [licenses/](licenses/).

## License
MIT for this project's code (see [LICENSE](LICENSE)); third-party components in [NOTICE](NOTICE).
