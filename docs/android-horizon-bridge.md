# Android-to-Horizon bridge: native-window subset and Vulkan surface seam

## Scope

The target is a homebrew NRO on Horizon OS. An Android APK cannot be launched by the Android Vulkan reference app or directly by Horizon: APK execution still needs package/manifest handling, an app-facing loader, and Android runtime APIs. This document records one narrow Switch-side platform seam, not an APK runtime.

## `ANativeWindow` function subset

`bridge/androidwine_native_window.h` keeps `ANativeWindow` opaque and declares a small NDK-shaped public function subset:

- `ANativeWindow_acquire` / `ANativeWindow_release`
- `ANativeWindow_getWidth` / `ANativeWindow_getHeight` / `ANativeWindow_getFormat`
- `ANativeWindow_setBuffersGeometry`

The bridge privately allocates an object tied to a Horizon `NWindow`; it tracks references, geometry readiness and a generation counter. In this prototype, `setBuffersGeometry` is a no-op for default/current geometry and returns `-EINVAL` for unsupported geometry or format changes. The smoke test exercises those calls and the surface-held reference lifetime. Internal `androidwine_*` helpers are bridge functions, not NDK exports.

This is **not** the Android platform-private `ANativeWindow` layout, a complete NDK surface API, or an implementation of Android buffer-queue semantics. The functions are currently linked directly into the Switch test NRO; there is no `libandroid.so` export table, native-library import resolver, Java `Surface` conversion, or Android Vulkan loader. Consequently the passing earlier synthetic-token tests do not establish ABI compatibility with an Android shared library.

## Vulkan translation boundary

An Android Vulkan application normally requests a surface through `vkCreateAndroidSurfaceKHR` using `VkAndroidSurfaceCreateInfoKHR.window`. The tested Horizon backend instead calls NXVK's `vkCreateViSurfaceNN` with a Horizon `NWindow` and `VkViSurfaceCreateInfoNN`:

```text
ANativeWindow* + VkAndroidSurfaceCreateInfoKHR
    -> direct AndroidWine C shim
    -> VkViSurfaceCreateInfoNN + vkCreateViSurfaceNN
```

The shim validates the request, rejects unsupported flags and non-null `pNext`, pins the window object for the Vulkan surface lifetime, and does not export `vkCreateAndroidSurfaceKHR` through a loader or make NXVK advertise `VK_KHR_android_surface`.

## Switch NRO tests and evidence

`bridge/androidwine_vk_surface_bridge_smoke.c` builds as `androidwine_vk_android_surface_bridge.nro` in the pinned NXVK GitHub Actions workflow. The current source test covers NDK-shaped queries, acquire/release, geometry updates, supported/unsupported `setBuffersGeometry` requests, malformed/unregistered surface requests, a 60-present VI swapchain path, surface-retained lifetime, and stale-window rejection. This new opaque-object/function-subset revision requires its own CI build and Switch run; prior hardware results below are for earlier source revisions only.

Two earlier revisions passed on the user's Switch, each with 60/60 presents:

1. Run [`39630991475`](https://github.com/berlint-hub/Android_wine/actions/runs/39630991475), artifact `androidwine-nxvk-vulkan-41be63f328c4cd01954a6fd437cfa44492f59573`: the original synthetic-pointer translation test.
2. Run [`39640954599`](https://github.com/berlint-hub/Android_wine/actions/runs/39640954599), artifact `androidwine-nxvk-vulkan-699679e247353df892550d95cfef29e5ae9f5387`: synthetic-pointer lifecycle/reference and geometry-accounting test. The supplied log confirmed zero-geometry rejection, balanced acquire/release, surface-held lifetime after owner release, unbind rejection while a surface was alive, stale-window rejection and 60/60 presents.

These passes are historical evidence for those exact synthetic-pointer revisions. They do not validate the current NDK-shaped object/functions or any Android ABI, loader, JNI/ART, NativeActivity, Unity or APK compatibility. Visual color change was not explicitly confirmed. The extra NRO includes GPL-2.0-or-later NXVK code; the workflow artifact retains the GPL source notice. AndroidWine bridge source is MIT-licensed.

## Remaining bridge work

1. **Build and hardware-test the current function subset.** Use a new Actions artifact and record its log separately; prior pass markers must not be transferred to this change.
2. **Native-library loader boundary.** Define ELF dependency/import resolution and callable symbol exports before attempting an Android `.so`. Report `VK_KHR_android_surface` only if a compatible loader dispatches it; explicitly reject unsupported extensions such as Android hardware-buffer import.
3. **NativeActivity/JNI/runtime.** Add lifecycle/input callbacks and a managed-runtime strategy. Surface translation alone cannot load or start a packaged app.
4. **Package fixture.** Use a self-authored native-only fixture first, then map DEX/ART and framework dependencies. Do not use a commercial APK as the first integration fixture.

The Android reference APK can help establish expected Android behavior, but running it on a phone is optional and neither required nor sufficient for the Switch-side work.

## Hardware rerun procedure

For the previous hardware-tested lifecycle artifact, copy `androidwine_vk_android_surface_bridge.nro` from artifact `androidwine-nxvk-vulkan-699679e247353df892550d95cfef29e5ae9f5387` to `sdmc:/switch/` and launch from Homebrew Menu as a **game override** (hold R while launching a game). It writes `sdmc:/androidwine_surface_bridge.log` plus `sdmc:/nvk_mesa.log`; that build presented changing colors for about one second, though the user did not report explicit visual confirmation.

For future revisions, use the artifact from the corresponding CI run and send both logs. A pass requires the lifecycle markers, `presented_frames=60 expected=60`, and `=== ANDROIDWINE_SURFACE_BRIDGE_CONTRACT PASSED ===`. Build success alone is not a hardware pass. The nine upstream NXVK hardware tests remain separate evidence from every adapter revision.
