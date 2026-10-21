# Android-to-Horizon bridge: first Vulkan surface spike

## Why this is a Switch-side step

The target is a homebrew NRO on Horizon. An Android APK cannot be launched by the Android Vulkan reference app or directly by Horizon: APK execution still needs package/manifest handling, a compatible loader and Android app/runtime APIs. This document defines the first *narrow platform-adapter seam*, not an APK runtime.

## Boundary under test

Android Vulkan windowed apps eventually ask Android's Vulkan entry point to create a surface from an `ANativeWindow`:

```text
VkAndroidSurfaceCreateInfoKHR.window
    -> vkCreateAndroidSurfaceKHR
```

The verified Switch backend instead creates a surface using NXVK's `VK_NN_vi_surface` and Horizon's `NWindow`:

```text
NWindow* from libnx
    -> vkCreateViSurfaceNN
```

The initial adapter in `bridge/androidwine_vk_surface_adapter.[ch]` makes the intended translation explicit:

```text
registered Android-window token + VkAndroidSurfaceCreateInfoKHR
    -> VkViSurfaceCreateInfoNN + vkCreateViSurfaceNN
```

It accepts one explicitly registered, opaque Android-window pointer, one Horizon-window pointer and NXVK's `vkCreateViSurfaceNN` dispatch function. It rejects malformed requests and unknown/stale windows rather than guessing. The shim is currently a direct C function call; it is **not** exported as `vkCreateAndroidSurfaceKHR`, does not make NXVK advertise `VK_KHR_android_surface`, and is not connected to Android's Vulkan loader.

## What the Switch NRO spike verifies

`bridge/androidwine_vk_surface_bridge_smoke.c` is built against the pinned NXVK driver as `androidwine_vk_android_surface_bridge.nro`. It creates a synthetic opaque window token, registers it to `nwindowGetDefault()`, verifies an unregistered token is rejected, sends an Android-shaped surface-create request through the adapter, then creates a swapchain and presents 60 frames through NXVK/VI.

The first reported Switch pass verified that the initial C adapter revision could translate this registered request into the already-working VI surface path on the target Switch. The current source revision additionally tests internal reference and geometry bookkeeping; it requires a fresh build and Switch run. **The synthetic token is not an Android `ANativeWindow` implementation.** The test does not run Android code, load an APK, provide ART/JNI/NativeActivity, expose Android Vulkan entrypoints or prove Unity/game compatibility.

The build is added to [the NXVK GitHub Actions workflow](../.github/workflows/build-nxvk-vulkan.yml), which also packages the existing nine upstream tests. CI run [`39630991475`](https://github.com/berlint-hub/Android_wine/actions/runs/39630991475) successfully built the first adapter revision. The reference-counting/geometry update was built in CI run [`39640954599`](https://github.com/berlint-hub/Android_wine/actions/runs/39640954599), artifact `androidwine-nxvk-vulkan-699679e247353df892550d95cfef29e5ae9f5387`; its hardware test is pending. The additional NRO links GPL-2.0-or-later NXVK code; the workflow artifact retains the GPL source notice. AndroidWine's adapter source itself remains MIT-licensed.

## Remaining bridge work (in order)

1. **Real native-window object and lifetime:** the adapter now models reference accounting and width/height/generation internally, and its updated Switch smoke test exercises that state. This is not Android's `ANativeWindow` ABI or callable `ANativeWindow_acquire/release` API. Next define the smallest ABI-compatible window proxy a hand-written native fixture requires; connect its callbacks to the tested accounting and Horizon `NWindow`, then cover resize, destruction and recreation on hardware.
2. **Android Vulkan loader boundary:** provide/import the app-facing Vulkan ABI and extension enumeration/dispatch. Only report `VK_KHR_android_surface` if the adapter is active; unsupported extensions such as Android hardware-buffer import must fail explicitly. This needs a loader/ELF dependency strategy before it can run any APK native library.
3. **NativeActivity/JNI/runtime:** add the lifecycle and input callbacks and then a managed-runtime strategy. Surface translation alone cannot load or start a packaged app.
4. **Package fixture:** use a self-authored native-only test first, then add DEX/ART and framework features only after their dependencies are mapped. Do not use a commercial APK as the first integration fixture.

The Android reference APK can help establish expected Android behavior, but running it on a phone is optional for the Switch adapter spike and does not substitute for the Switch NRO test.

## Hardware result and rerun procedure

The user ran the first revision of `androidwine_vk_android_surface_bridge.nro` on Switch hardware. The supplied log reports successful NXVK initialization, correct rejection of an unregistered window and unsupported flags/pNext, successful adapter call to VI surface creation, present support, 1280×720 swapchain with three images, and **60/60 frames presented**. It ends with `=== ANDROIDWINE_SURFACE_BRIDGE_CONTRACT PASSED ===`. This is a hardware pass for that synthetic adapter contract only; the current lifecycle-accounting revision and the actual `ANativeWindow` ABI/APK runtime remain untested/unimplemented.

The previous artifact `androidwine-nxvk-vulkan-41be63f328c4cd01954a6fd437cfa44492f59573` from [run `39630991475`](https://github.com/berlint-hub/Android_wine/actions/runs/39630991475) contains the already-tested first revision. The updated lifecycle revision is artifact `androidwine-nxvk-vulkan-699679e247353df892550d95cfef29e5ae9f5387` from [CI run `39640954599`](https://github.com/berlint-hub/Android_wine/actions/runs/39640954599). For either build, copy `androidwine_vk_android_surface_bridge.nro` to `sdmc:/switch/` and launch it from Homebrew Menu as a **game override** (hold R while launching a game). It presents changing colors for about one second and writes `sdmc:/androidwine_surface_bridge.log` plus `sdmc:/nvk_mesa.log`. The first revision's log ended with:

The updated lifecycle build should additionally log `window_without_geometry=correctly_rejected`, `window_references=acquire/release balanced`, `unbind_with_live_surface=correctly_rejected`, `activity_window_released=surface_reference_keeps_window_alive`, and `window_lifecycle=owner-release, surface-retain, surface-destroy, stale-reject PASSED` before the final pass marker.

```text
unregistered_window=correctly_rejected
surface_translation=VK_KHR_android_surface request -> VK_NN_vi_surface
presented_frames=60 expected=60
=== ANDROIDWINE_SURFACE_BRIDGE_CONTRACT PASSED ===
```

Build success alone is not a hardware pass. The first revision's user hardware pass and the nine upstream NXVK passes are separate evidence; do not transfer either pass to the updated lifecycle build until that NRO is run.
