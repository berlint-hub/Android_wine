# Android compatibility runtime for Nintendo Switch

An experimental homebrew project exploring how to run a **small, explicitly defined subset of Android applications** on Nintendo Switch under Horizon OS. The long-term goal is to grow compatibility incrementally; this is not currently a general-purpose Android runtime.

> **Status:** project planning / architecture stage. No runtime or APK execution support exists yet.

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
3. **Runtime research and smallest executable test** — choose one self-authored Android test app and define the exact API/ABI subset it needs before implementing execution. Loading a `.so` alone does not count as APK support.
4. **Incremental compatibility** — add only the Android APIs required by the test app, with explicit unsupported-feature reporting.

See [docs/architecture.md](docs/architecture.md) and [docs/milestones.md](docs/milestones.md).

## Build status

A minimal libnx probe can be built with devkitPro using `make`. It writes a diagnostic log to the SD card and checks basic Horizon services, memory reporting, and controller input. See [docs/switch-probe.md](docs/switch-probe.md). It does **not** launch a game or APK. The build has not been verified in this sandbox because devkitPro/libnx is not installed here.

## Legal and safety

Use apps and assets you are legally entitled to use. Do not use this project to bypass app protections, licensing, or platform security. APK handling and compatibility should remain separate from obtaining application files.