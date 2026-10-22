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

## Observed fixture inventory and first loader target

The user supplied `llvm-readelf` output for the fixture, SHA-256 `eb23c21bb7ff36eed6a04cbdf371a01c770898a2472306478d0d6c0797a28213`. It is an ELF64, little-endian, AArch64 `ET_DYN` shared object with SONAME `libandroidwine_loader_fixture.so`.

Its direct `DT_NEEDED` list is `libandroid.so`, `libvulkan.so`, `libm.so`, `libdl.so`, and `libc.so`. The current `.dynsym` has one exported entrypoint, `androidwine_loader_fixture_probe_window`, and ten undefined function imports:

- `ANativeWindow_acquire`, `ANativeWindow_release`, `ANativeWindow_getWidth`, `ANativeWindow_getHeight`, `ANativeWindow_getFormat`, `ANativeWindow_setBuffersGeometry`
- `vkGetInstanceProcAddr`
- `__cxa_finalize@LIBC`, `__cxa_atexit@LIBC`, `__register_atfork@LIBC`

The object has `BIND_NOW` / `NOW`, so the PLT imports need eager resolution for this fixture. `.rela.dyn` contains three `R_AARCH64_RELATIVE` entries; `.rela.plt` contains ten `R_AARCH64_JUMP_SLOT` entries. It also uses `DT_GNU_HASH`, `DT_VERSYM`/`DT_VERNEED` (one required version), `DT_FINI_ARRAY` (16 bytes), and standard RELA dynamic tags. No packed Android relocations appear in this fixture's supplied inventory. These observations describe this exact library only, not arbitrary Android NDK output.

A bounded first loader target can therefore be explicitly fixture-driven: ELF64 little-endian AArch64 `ET_DYN`; checked `PT_LOAD`/`PT_DYNAMIC` mapping; the observed GNU-hash, dynamic-symbol/string and symbol-version metadata; RELA handling limited to `R_AARCH64_RELATIVE` and `R_AARCH64_JUMP_SLOT`; eager resolution; and only the named SONAME/symbol allowlist. `libandroid.so` and `libvulkan.so` would map to AndroidWine shims. The fixture's `libc.so` hooks need an explicit, documented compatibility decision; `libm.so`/`libdl.so` must not silently become general Bionic implementations merely because they appear in `DT_NEEDED`. Unsupported tags, relocations, versions, imports, TLS, and dependencies must be rejected with diagnostics. This is a proposed first slice, not implemented behavior.

## Safe first fixture and evidence ladder

A self-authored ARM64 Android shared-library fixture (not a commercial APK) is built in GitHub Actions. Push run [`39739901266`](https://github.com/berlint-hub/Android_wine/actions/runs/39739901266) and PR run [`39739909084`](https://github.com/berlint-hub/Android_wine/actions/runs/39739909084) produced artifact `androidwine-loader-fixture-28eb9c687bfc17279f0e12a159e772c19710d0c1`, containing the `.so` and an `llvm-readelf` inventory. Later push run [`39740194729`](https://github.com/berlint-hub/Android_wine/actions/runs/39740194729) produced the same content and publishes the inventory in its Actions summary. The user supplied the inventory, which is recorded above. The workflow builds and inspects but does not execute the library. Remaining evidence stages are:

1. **Inspection baseline:** the CI `llvm-readelf` inventory for the fixture is reviewed above. Implement a host-side bounds-checked inspector and malformed/truncated-input tests; do not execute parsed files.
2. **Loader proof:** after that parser is tested, load a fixture that uses only explicitly implemented relocations and a tiny set of stubs; call one entrypoint and capture each import-resolution decision in a debug log. This is a native-module loader test, not APK support.
3. **NDK window proof:** resolve the supported `ANativeWindow_*` imports through the AndroidWine API module, call them from the fixture, and verify balanced references/geometry on Switch.
4. **Vulkan proof:** only after a real loader can resolve the frontend, call the app-facing proc resolver, enumerate the supported Android-facing extensions, create a VI-backed Android surface, and present a frame. Keep this separate from the current directly linked NRO evidence.
5. **Application runtime:** NativeActivity lifecycle, JNI/ART, framework/resource services, package parsing, and app launch remain later, separate gates.

The checked-in fixture is intentionally small and its Android NDK build is only an input-generation step; running an APK on a phone is not required for Switch-side loader work.

## Immediate implementation decision

The inventory review selected the candidate first loader scope above. A host-only metadata inspector now has strict file/segment/table bounds, GNU-hash/symbol/version/RELA parsing, and synthetic valid/malformed-input tests. CI run [`39754119776`](https://github.com/berlint-hub/Android_wine/actions/runs/39754119776) passed those tests and parsed the NDK-generated `.so` alongside `llvm-readelf`; the inspector emits a JSON inventory and never executes ELF files. A separate loader prototype should follow only after the `@LIBC` hook behavior and allowlisted `DT_NEEDED` handling are explicitly designed.
