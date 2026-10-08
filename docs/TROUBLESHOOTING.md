# Troubleshooting

| symptom | what to check |
|---|---|
| Game does not start / window flashes | `~/Zomboid/pz-arm64.log` and the tail of `~/Zomboid/projectzomboid.sh.log`; `hs_err_pid*.log` is written to the game dir by the JVM on a crash |
| `runtime for profile … is not installed` | rerun `install.sh` (without `--no-b41` / `--no-b42`) |
| Black or missing 3D lighting | `LightingJNI` / `libLighting64.so` did not load – look for `[pzb]` or `UnsatisfiedLinkError` lines in the log |
| Tutorial pad errors / phantom controller | a touch screen is seen as a gamepad; keep `hide_touchscreens` enabled |
| Crash with `SIGSEGV … natives/libPZ…so` | re-run with `"box64_env": {"PZ_TRACE": "1"}` in `pz-arm64.json` and attach the log |
| Out of memory | raise `"xmx"` (e.g. `"4096m"`) if the device has the RAM |
| Stale bindings after a game update | delete `~/.cache/pz-arm64` (the cache key already includes the game's file sizes/dates) |

When reporting a bug include: device/SoC, distribution, Mesa version, game build (`version=` line in the log), and the logs above.
