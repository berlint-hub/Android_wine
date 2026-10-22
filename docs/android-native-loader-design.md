# Android native loader and Vulkan boundary — design

## Purpose and status

The Horizon-side surface seam is hardware-tested: an AndroidWine-owned opaque window object and a small NDK-shaped function subset translate the test's Android-shaped Vulkan surface request to NXVK/Horizon VI. The next step is to make an Android native library *callable* without implying that an APK or Android application runtime exists.

This document defines proposed boundaries and tests. It does not implement an ELF loader, `libandroid.so`, `libvulkan.so`, Bionic, JNI/ART, NativeActivity, or APK launching.

## Keep four layers separate

```text
user-owned package / ELF files
    -> bounded package and ELF inspection
    -> Android AArch64 ELF loader + deliberately small native API/import shims
    -> Android-facing Vulkan ABI/dispatch shim
    -> NXVK Vulkan backend -> Horizon VI / NWindow
```

1. **Package and ELF inspection.** Treat APK/ELF input as untrusted. Read metadata, dependencies, undefined imports, symbol versions, and relocations without running the file. Do not conflate “ELF parsed” with “ELF loaded.”
2. **ELF loading and imports.** A future runtime must load only user-provided Android AArch64 `ET_DYN` modules, resolve a documented set of shared-library imports, apply an explicitly supported relocation set, and define constructors/destructors, `dlopen`/`dlsym`, TLS, and unwind behavior as the chosen fixture requires. Reject unsupported ABI features with the module name and symbol/relocation in the debug log. Do not claim Bionic compatibility from a few shims.
3. **Android-facing native API.** The tested `ANativeWindow` functions remain an AndroidWine-owned subset. They can become exports in a future `libandroid.so` compatibility module, but the existing directly linked NRO functions are not yet dynamically resolvable imports.
4. **Vulkan frontend and backend.** The app-facing Vulkan entrypoints/extension list are a separate compatibility layer from the NXVK backend. The current surface adapter is a direct C call, not a Vulkan loader.

## Vulkan dispatch policy

The app-facing layer must own capability negotiation; it must not blindly forward Android extension names to NXVK.

- `VK_KHR_android_surface` may be reported to a native module only when the AndroidWine `vkCreateAndroidSurfaceKHR` path, surface destruction/query functions, and present/queue integration are callable through the app-facing dispatch.
- The frontend translates `VkAndroidSurfaceCreateInfoKHR` to the tested `VkViSurfaceCreateInfoNN` path. The underlying instance enables the backend's required surface extension; it must not receive `VK_KHR_android_surface` unless NXVK independently advertises it.
- Keep the internal `VK_NN_vi_surface` backend detail out of the Android app-facing extension list.
- `vkGetInstanceProcAddr` and `vkGetDeviceProcAddr` need wrappers for intercepted entrypoints and forwarding for supported backend entrypoints. Extension enumeration, instance/device creation, and returned function pointers must agree; advertising an extension while returning no callable function is a failure.
- Android hardware-buffer import and other unimplemented external-memory paths stay unadvertised and fail explicitly. A successful VI present test is not evidence for those features.

## Safe first fixture and evidence ladder

Build a self-authored ARM64 Android shared-library fixture (not a commercial APK) that has one exported entrypoint and an intentionally small, known import surface. Use the Android NDK in GitHub Actions to record its ELF metadata. The initial stages should be:

1. **Inspect only:** report ELF class/endianness/machine, `DT_NEEDED`, undefined dynamic symbols, symbol versions, and relocation types. Add malformed-file and bounds tests. Do not execute it.
2. **Loader proof:** load a fixture that uses only explicitly implemented relocations and a tiny set of stubs; call one entrypoint and capture each import-resolution decision in a debug log. This is a native-module loader test, not APK support.
3. **NDK window proof:** resolve the supported `ANativeWindow_*` imports through the AndroidWine API module, call them from the fixture, and verify balanced references/geometry on Switch.
4. **Vulkan proof:** only after a real loader can resolve the frontend, call the app-facing proc resolver, enumerate the supported Android-facing extensions, create a VI-backed Android surface, and present a frame. Keep this separate from the current directly linked NRO evidence.
5. **Application runtime:** NativeActivity lifecycle, JNI/ART, framework/resource services, package parsing, and app launch remain later, separate gates.

The first fixture should be tiny enough that its required imports and relocations can be reviewed before implementation. Its Android-side build is an input-generation step; running an APK on a phone is not required for the Switch-side loader test.

## Immediate implementation decision

Before writing loader code, build and inspect the self-authored `.so` fixture and commit its exact dependency/import/relocation inventory. Use that inventory to select the first loader subset; do not guess a broad relocation or Bionic compatibility target. Keep the parser/inspector host-testable and keep execution behind a separate opt-in loader test.
