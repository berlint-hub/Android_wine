# Architecture notes

## Runtime boundary

The target is a homebrew `.nro` running on Horizon OS, not Android running as the console's operating system. The runtime must provide or adapt Android-facing behavior itself and must use supported homebrew interfaces for the Switch side.

Keep host-side tools separate from code that runs on the console. Host tools make APK parsing and test-fixture generation easier to validate without requiring a Switch for every change.

## What the NetherSX2 port demonstrates

The linked [NetherSX2_nx branch](https://github.com/berlint-hub/NetherSX2_nx/tree/arena/2e44eb02-nethersx2-nx) is a useful example of a *specific native Android program* being wrapped for Horizon. Its README says it bundles and runs NetherSX2's Android-built native emulator core (`libemucore.so`) as part of a native `.nro`, with Switch-side graphics and compatibility shims. It provides the interfaces that this one core needs; it does not load or execute an Android APK, ART, or arbitrary Android apps. PS2 BIOS and game files are inputs to the PS2 emulator, not Android apps.

This approach can be reused for a compatible native Android program when its core can be obtained/built and its platform dependencies are known. It cannot be generalized to arbitrary APKs merely by providing a core plus an APK: the APK may contain Java/Kotlin DEX code, native libraries, resources, and manifest metadata, and its activities expect ART and Android framework services. Running that class of APK requires implementing or porting those runtime services too.

## Proposed components

```text
Host development machine
  apk-inspect       Parse APK/ZIP metadata and report likely requirements
  test fixtures     Self-authored APKs and small synthetic ZIP/ELF test inputs

Nintendo Switch (Horizon OS / homebrew)
  NRO bootstrap     Lifecycle, screen, input, filesystem, logging
  APK/package layer APK contents, manifest/resource metadata, app data layout
  Android runtime   ART + the supported Android framework/API subset (future)
  Native ABI layer  JNI, Android/Bionic-facing native APIs (future)
  Platform adapters Graphics, audio, input, storage, timing, lifecycle
```

This is a conceptual split, not a claim that the components already exist or that a particular upstream runtime can be ported unchanged.

## Important technical constraints

- **APK is a package format, not a directly executable file.** It may contain DEX bytecode, resources, manifests, and native libraries for one or more ABIs.
- **ARM64 compatibility is necessary but insufficient.** Android native libraries expect Android's linker/Bionic and often JNI or framework services; Horizon's native environment is different.
- **Java/Kotlin apps require a managed runtime and framework.** ART and Android APIs/services are a major project area. A loader for ELF shared objects does not provide this.
- **Graphics are not automatically interchangeable.** Android apps may expect EGL/OpenGL ES or Vulkan and Android surface/window behavior. The Switch adapter has to define supported APIs and lifecycle behavior.
- **Memory and scheduling are constrained.** The runtime must fail clearly when an app's memory, threading, or service requirements exceed what the homebrew environment can provide.
- **Compatibility must be measured per feature.** Maintain a capability matrix and test each supported API; avoid presenting one passing game as general APK support.

## First reference app: Unity Android smoke test

`unity-smoke/` is a self-authored Unity project and the first Android-side reference fixture. Its Editor build helper selects an ARM64 IL2CPP player (min API 24, target API 35). At runtime it renders a diagnostic screen, reads one `StreamingAssets` file, writes/reads app-private storage, handles touch input, exercises pause/focus callbacks, and makes JNI calls to a few stable Java/Android classes. See its README for build and device-test steps.

This reference app answers the narrower question “does this Unity-generated Android app behave normally on a known Android device?” It does not itself test Horizon compatibility. The next architecture gate is to map each observed operation to a proposed Horizon implementation, identify what is absent, and only then choose a runtime strategy. Its external Google/Facebook/ads/billing/media services are deliberately excluded; Royal Kingdom's integrations remain a separate, later compatibility challenge.

## Design principles

- Prefer small interfaces and independently testable host-side code.
- Do not bundle third-party Android system images or proprietary application files.
- Treat APKs as untrusted input: bound archive sizes/counts, validate paths, and reject malformed metadata.
- Document supported API levels, ABIs, graphics paths, and known failures as they become real and tested.