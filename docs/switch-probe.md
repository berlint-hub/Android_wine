# Switch hardware probe

This is the first runnable foundation for the project. It is a small libnx homebrew program, not an Android runtime or game launcher yet. It records basic target-device diagnostics so we can confirm the Switch build and use environment before adding an app compatibility layer.

## Build

Build on a machine with devkitPro, devkitA64, and libnx installed and configured:

```sh
make clean
make
```

The expected output is `androidwine_probe.nro` in the repository root. The current coding environment does not have devkitPro installed, so this NRO has not been built or hardware-tested here.

## Build with GitHub Actions

`.github/workflows/build-switch.yml` builds the probe in the devkitPro Switch container on pushes, pull requests, or manual dispatch. After the workflow completes, open **Actions → Build Switch probe → the successful run → Artifacts** and download `androidwine-probe-<commit-sha>`. The archive contains `androidwine_probe.nro`. The workflow must be present on GitHub and the branch pushed before it can run.

## Run on Switch

1. Copy `androidwine_probe.nro` to the SD card, for example `sdmc:/switch/androidwine_probe.nro`.
2. Launch **AndroidWine Probe v0.2** from the Homebrew Menu. The screen must show `AndroidWine Switch probe v0.2.0` and `SD mount fix build`; if it shows the old "SD mount failed" text, the old NRO is still being launched. For a full-memory test, launch it as a game override (hold **R** while launching a game) if your homebrew setup supports that.
3. Wait for the probe screen. Press **A** once to add a marker to the log; press **Plus** to exit cleanly.
4. Copy `sdmc:/switch/androidwine/androidwine.log` off the card and share that text log.

The log appends a new boot/exit section on each run. It records whether libnx's existing `sdmc:` devoptab is available, memory-query results, and button transitions. The probe does not call `fsdevMountSdmc()` again: libnx normally mounts the SD device before an NRO enters `main()`, and a redundant mount can report an error despite the existing mount being usable. Each diagnostic event is flushed and synced to the SD card where the filesystem supports it. It does not capture a Switch fatal-error report if the process or console crashes before the runtime can log the failure.

## What to send back

Please send:

- the complete `androidwine.log`;
- whether it was launched from Album/applet mode or as a game override;
- whether the probe exited normally, froze, or produced a Horizon error screen;
- if you already have a target Android game in mind, its name/version and whether the package includes `arm64-v8a` native libraries.

Please do not send copyrighted game APKs or assets. This probe cannot launch the game yet; the log is the first hardware check. Once we have a specific target app and its execution model, we can decide what compatibility code is needed and add app-specific diagnostics.