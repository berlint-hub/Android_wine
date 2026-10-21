# Android compatibility runtime for Nintendo Switch

An experimental homebrew project exploring how to run a **small, explicitly defined subset of Android applications** on Nintendo Switch under Horizon OS. The long-term goal is to grow compatibility incrementally; this is not currently a general-purpose Android runtime.

> **Status:** Horizon diagnostic NRO, the nine-test NXVK Vulkan feature ladder, and the first synthetic Android-surface-to-Horizon adapter NRO have passed on the user's Switch. A follow-up adapter revision adds lifecycle/reference accounting and now needs its own CI build and device run. Neither revision provides an Android loader/runtime or APK execution.

## Target

- Nintendo Switch running Horizon OS with custom firmware and homebrew support.
- Homebrew packaged as an `.nro` and built with the devkitPro/libnx toolchain.
- Android APKs and app assets are supplied by the user. This repository will not distribute proprietary apps, system images, or copyrighted assets.

This is **not** an Android OS port. It does not assume that Android is installed on the console, and it is not intended to replace Horizon OS.

## Reality check

Two Switch projects illustrate different approaches:

- [angrybirds2_nx](https://github.com/NaGaa95/angrybirds2_nx) is a focused port of one Android game. Its README describes loading that game's ARM64 libraries in a minimal compatibility environment.
- [NetherSX2_nx](https://github.com/berlint-hub/NetherSX2_nx/tree/arena/2e44eb02-nethersx2-nx) wraps the Android-built NetherSX2/PCSX2 native emulator core (`libemucore.so`) in a Switch `.nro`. The port bundles the core and native Switch graphics components and supplies the specific Android/Bionic/POSIX behavior that core expects. It is **not** an Android emulator and does not launch an APK; users provide PS2 BIOS and game files instead.

These are good models for porting a known native Android app/core: replace or emulate the platform interfaces that that particular program uses. An APK is not a generic emulator core or just a data file. A general APK runtime needs substantially more than an ARM64 shared-library loader: APK/resource and manifest handling, Android's Java runtime (ART), Android framework APIs and services, JNI, Bionic/native APIs, and graphics, audio, input, storage, and lifecycle integration. Horizon OS does not provide those Android services. Compatibility will therefore be partial and built in small, testable layers.

## Initial development strategy

1. **Horizon homebrew shell** — the initial probe is in place; build and run it on hardware to verify display/input/storage/memory diagnostics.
2. **Host-side APK inspection tool** — list package metadata, ABI variants, native libraries, and assets without executing the APK. This gives us a safe, testable way to characterize a target app.
3. **Unity Android smoke-test fixture** — `unity-smoke/` contains a self-authored Unity project that tests ARM64 IL2CPP startup, JNI, packaged assets, persistent storage, touch input, and lifecycle on Android. It establishes a reference app; it does not run on Switch.
4. **Switch Vulkan path** — build and hardware-test the complete nine-test pinned NXVK validation ladder (compute, graphics/readback, textures, depth, blend, cubemap, and present). All nine currently pass on the user's Switch; see [docs/nxvk-vulkan-test.md](docs/nxvk-vulkan-test.md).
5. **Android-to-Horizon Vulkan surface spike** — the first direct C seam maps a registered opaque Android-window token to Horizon `NWindow`, then translates an Android-shaped surface-create request to NXVK `VK_NN_vi_surface`. Build/test it as its own Switch NRO; see [docs/android-horizon-bridge.md](docs/android-horizon-bridge.md). This does not launch APKs or provide an Android loader.
6. **Optional Android Vulkan reference** — [android-vulkan-smoke/](android-vulkan-smoke/README.md) measures Android loader/surface/present behavior on an Android device. It is a reference only, not a prerequisite for running Switch-side tests.
7. **Runtime research and smallest executable test** — define a realistic execution subset before implementing APK execution. Loading a `.so` alone does not count as APK support.
8. **Incremental compatibility** — add only the Android APIs required by the test app, with explicit unsupported-feature reporting.

See [docs/architecture.md](docs/architecture.md) and [docs/milestones.md](docs/milestones.md).

## Build status

A minimal libnx probe can be built with devkitPro using `make`. It writes a diagnostic log to the SD card and checks basic Horizon services, memory reporting, and controller input. See [docs/switch-probe.md](docs/switch-probe.md). It does **not** launch a game or APK. GitHub Actions builds this diagnostic NRO. The [NXVK Vulkan test guide](docs/nxvk-vulkan-test.md) documents the successful nine-test Switch hardware run.

The experimental Switch-side surface adapter source is in [bridge/](bridge/) and its limits/hardware procedure are in [docs/android-horizon-bridge.md](docs/android-horizon-bridge.md). CI and Switch hardware tests passed for the synthetic window-token contract; actual Android `ANativeWindow`, loader, JNI/ART and APK execution remain unimplemented.

The Android-only Vulkan reference APK lives in [android-vulkan-smoke/](android-vulkan-smoke/README.md), with its own pinned Android SDK/NDK GitHub Actions build. Its APK has not yet been run on an Android device. The self-authored Unity Android reference project is separate in [unity-smoke/](unity-smoke/README.md); it needs a licensed Unity 2022.3 LTS Android editor installation and remains unbuilt/untested.

## Legal and safety

Use apps and assets you are legally entitled to use. Do not use this project to bypass app protections, licensing, or platform security. APK handling and compatibility should remain separate from obtaining application files.