# Unity Android smoke-test project

This is a small, self-authored Unity project intended to produce a diagnostic Android APK. It is a **future compatibility fixture**, not a Switch launcher and not evidence that Unity or Royal Kingdom runs on Horizon. The current Switch NRO still only probes Horizon services.

The fixture is deliberately small and exercises the Unity/Android boundary seen in the Royal Kingdom inventory:

- Unity IL2CPP/ARM64 Android player startup (selected by the build script);
- Android JNI calls to `java.lang.System`, `android.os.Build.VERSION`, and `android.util.Log`;
- loading a packaged `StreamingAssets` file from Android's APK-backed asset location;
- persistent-data file write/read;
- touch/mouse input, pause/focus lifecycle callbacks, and visible on-screen diagnostics.

It does **not** include Unity binaries, Android system images, the Royal Kingdom APK/assets, or Google/Facebook/billing/ads SDKs. Those are intentionally out of scope for the first smoke test.

## Build the fixture APK

Requirements: Unity **2022.3 LTS** with Android Build Support, Android SDK/NDK, and OpenJDK modules installed. Open this directory as a Unity project. In Unity's command line, run:

```sh
Unity \
  -batchmode -quit \
  -projectPath "$PWD/unity-smoke" \
  -executeMethod AndroidWine.SmokeTestBuild.BuildAndroid
```

On Windows, use the same arguments with the full path to `Unity.exe` and a Windows project path. The Editor build helper configures Android ARM64 + IL2CPP, minimum API 24, target API 35, creates the single-scene build settings entry, and writes `Build/AndroidWineUnitySmoke.apk`.

Unity editor licensing and Android modules are not available in the repository's GitHub Actions toolchain. Therefore the existing GitHub Actions workflow continues to build only the Horizon diagnostic NRO; it does not claim to build this APK. Bring back the APK build log and device log if the Unity Editor reports a build issue.

## Android-side test procedure

1. Install and launch `AndroidWine Unity Smoke` on an ARM64 Android device/emulator with Android 7.0 (API 24) or newer.
2. Confirm the screen reports Unity startup, Java/JNI status, packaged asset status, and persistent storage status.
3. Tap the screen: the displayed tap counter should increment and `unity-smoke.log` should record the event.
4. Background and resume the app, then copy `unity-smoke.log` from the app's `Application.persistentDataPath` (the screen also displays this path).
5. Share the log and exact device/API/ABI. Do not share unrelated app data.

## What this test can and cannot establish

A passing run proves this small Unity-generated APK works on that Android device. It gives us a known-good behavioral checklist and an ARM64 Unity baseline to compare against a future Horizon implementation. It does not execute on Switch, implement Android APIs on Horizon, load an arbitrary APK, or establish Royal Kingdom compatibility. Royal Kingdom's additional Google/Facebook/ads/billing/media integrations remain untested.
