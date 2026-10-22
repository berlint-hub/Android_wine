# Android Vulkan reference smoke APK

This is a small **Android-side reference test**, not an Android runtime for Horizon OS and not a Switch APK. It isolates the Android loader/platform path that the native NXVK Switch tests do not exercise:

- Android Vulkan loader and `VK_KHR_android_surface` discovery;
- `ANativeWindow` → `vkCreateAndroidSurfaceKHR`;
- graphics queue and Android-surface present support;
- `VK_KHR_swapchain`, image acquisition, command submission and present;
- 180 sequential present frames, followed by a solid green pass frame.

It does not load or run APKs on Switch, implement an Android-to-Horizon bridge, test JNI/Unity, or establish Vulkan driver compatibility between Android and NXVK. The Switch native NXVK tests use the Switch-specific `VK_NN_vi_surface`; this APK tests Android hardware only. The Unity fixture remains separate and has not been built/device-tested.

## Build

The reproducible CI build is `.github/workflows/build-android-vulkan-smoke.yml`. It pins Java 17, Gradle 8.9, Android platform/build tools 35, NDK `27.2.12479018`, CMake `3.22.1`, and builds only `arm64-v8a`. Run it through GitHub Actions (`workflow_dispatch`) or push a change under this directory. The first successful build is run [`39622901010`](https://github.com/berlint-hub/Android_wine/actions/runs/39622901010). Download its artifact `androidwine-android-vulkan-smoke-e5cbcbe0d3ab92e3305e9beba17b5ae8df047d20` (and future `androidwine-android-vulkan-smoke-<commit>` artifacts); it contains the debug APK and `SHA256SUMS.txt`.

With those components installed locally:

```sh
cd android-vulkan-smoke
gradle --no-daemon :app:assembleDebug
```

APK output: `app/build/outputs/apk/debug/app-debug.apk`.

## Run on an Android device

Use an Android 7.0 / API 24+ arm64 device with Vulkan support. Enable USB debugging and connect with `adb`:

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am force-stop org.androidwine.vulkansmoke
adb shell monkey -p org.androidwine.vulkansmoke 1
```

Wait about five seconds. A successful run presents 180 frames and leaves a green screen. Tap the screen or press Back to exit. Capture both log sources before closing the app (the private app log is persisted until the next launch overwrites it):

```sh
adb logcat -d -s AndroidWineVkSmoke:I '*:S' > android-vulkan-smoke-logcat.txt
adb exec-out run-as org.androidwine.vulkansmoke cat files/vulkan-smoke.log > android-vulkan-smoke-device.log
```

A complete pass ends with `presented_frames=180` and `=== ANDROID_VK_PROBE PASSED ===`. Preserve the device make/model, Android build/API level, Vulkan implementation/driver version (if shown by the device), and both logs when reporting a failure. The logs also identify the last successful stage (`vkCreateInstance`, Android surface, device, swapchain, acquire/submit/present).

The app intentionally leaves Vulkan optional in the manifest: devices with no usable Vulkan loader can still launch the probe and record a useful failure instead of being filtered from installation. A failed probe waits for touch or Back so the diagnostic can be retrieved.
