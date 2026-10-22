# Milestones

Milestones are gates, not promises of universal compatibility. Each should have a reproducible test and a documented unsupported-feature list.

## 0 — Build and hardware proof

- [ ] Select and document the devkitPro/libnx toolchain version.
- [x] Build a minimal `.nro` from a clean checkout (GitHub Actions, run `39582225272`).
- [x] Run it on a Switch with homebrew support; verify controller input, SD logging, and normal exit (user-supplied run log, 2026-10-21).
- [x] Add logging and a clean exit path.

**Exit condition:** a reproducible build plus a recorded manual test procedure. This proves only the homebrew shell, not Android compatibility.

## 0.5 — Switch Vulkan/NXVK proof

- [x] Pin an NXVK revision and define the compute, shader, texture, depth, indexed-draw, blend, mipmap, cubemap, and present tests.
- [x] Build the complete nine-app Vulkan feature ladder through GitHub Actions (run `39607675161`, artifact `androidwine-nxvk-vulkan-e357e8852e5f811ca0e05f30940ea01c63075fc6`).
- [x] Run all nine validation tests on hardware; user logs report pass for compute, shader/readback, texture/sampling, depth, indexed draws, blending, mipmaps, BC1, cubemap, and 300/300 swapchain presents.
- [x] Record that this proves native NXVK/Horizon Vulkan functionality only—not Android Vulkan loading/surfaces, Unity startup, JNI, or APK execution.

**Test instructions:** [docs/nxvk-vulkan-test.md](nxvk-vulkan-test.md). The smoke NROs statically link NXVK and are subject to its GPL-2.0-or-later terms.

## 0.6 — Android Vulkan reference (not Switch support)

- [x] Add a minimal NativeActivity APK probing Android Vulkan loader/extensions, `ANativeWindow` surface, graphics/present queue, swapchain, and 180 clear/present frames.
- [x] Add a pinned GitHub Actions Android SDK/NDK/Gradle build and reproducible install/log-capture instructions.
- [x] Build the arm64 debug APK in GitHub Actions (run `39622901010`; artifact `androidwine-android-vulkan-smoke-e5cbcbe0d3ab92e3305e9beba17b5ae8df047d20`).
- [ ] Run on an Android arm64 device; retain device details and logcat/private app logs.
- [ ] Compare Android results with the already-passing native NXVK `VK_NN_vi_surface` tests; do not infer an Android-to-Horizon bridge from either result alone.

**Test instructions:** [android-vulkan-smoke/README.md](../android-vulkan-smoke/README.md). This is an Android reference only; it neither launches APKs on Switch nor tests Unity/JNI. The phone-side reference run is optional for the Switch-side bridge work.

## 0.7 — First Android surface adapter spike (Switch-side)

- [x] Define a direct C adapter contract from a registered opaque Android-window handle and `VkAndroidSurfaceCreateInfoKHR` to NXVK `vkCreateViSurfaceNN` plus Horizon `NWindow`.
- [x] Reject malformed requests and stale/unregistered window handles explicitly.
- [x] Add a dedicated NRO smoke test that uses the adapter, creates a VI swapchain and attempts 60 presents; keep it separate from the nine upstream NXVK pass claims.
- [x] Build and publish the adapter NRO through the NXVK GitHub Actions workflow (run `39630991475`; artifact `androidwine-nxvk-vulkan-41be63f328c4cd01954a6fd437cfa44492f59573`).
- [x] Run the new NRO on the user's Switch; user log reports a passing adapter contract and 60/60 presents (2026-10-21).
- [x] Record that this revision's synthetic token is only an adapter test, not an Android ABI, Vulkan loader, or APK/NativeActivity runtime. Later object/function work is tracked separately below.

**Scope and test instructions:** [docs/android-horizon-bridge.md](android-horizon-bridge.md). The passed revision used a synthetic pointer token. It is not an `ANativeWindow` ABI, Android loader export, JNI/ART implementation, or APK runner.

## 0.8 — Window lifecycle accounting (Switch-side prototype)

- [x] Add owner/window reference accounting, geometry and generation state, surface-held references, and paired surface destruction to the internal adapter contract.
- [x] Extend the Switch NRO to test geometry-not-ready rejection, acquire/release balance, Activity-owner release while a Vulkan surface keeps the window alive, surface destruction, and stale-window rejection.
- [x] Build the updated lifecycle NRO in GitHub Actions (run `39640954599`; artifact `androidwine-nxvk-vulkan-699679e247353df892550d95cfef29e5ae9f5387`).
- [x] Run the updated NRO on Switch; user log reports all lifecycle markers and 60/60 presents (2026-10-21).
- [x] Implement a minimal public NDK-shaped `ANativeWindow` object/function subset; keep loader and JNI integration as separate gates (completed and separately tested in 0.9).

**Caveat:** this hardware-tested revision still uses a synthetic pointer token. Its counters and geometry fields are not Android's ABI implementation and do not provide APK/NativeActivity support.

## 0.9 — Opaque NDK-shaped ANativeWindow subset (Switch-side prototype; passed)

- [x] Replace the smoke test's token with a privately allocated, opaque AndroidWine window object linked to Horizon `NWindow`.
- [x] Implement a narrow public NDK-shaped function subset: acquire/release, width/height/format queries, and `setBuffersGeometry` default/current-size behavior.
- [x] Extend the NRO test for geometry readiness, supported and unsupported geometry requests, lifetime retention, stale use, and 60 VI presents.
- [x] Build this revision in GitHub Actions run `39668507515` (push) and `39668520496` (PR); artifact `androidwine-nxvk-vulkan-4aed0fe071a377a81849a298b35ec431e2e6b760`.
- [x] Run the matching artifact on Switch; supplied log reports all function/lifecycle markers, 1280×720 swapchain, and 60/60 presents (2026-10-21).
- [x] Document the Android native-module, Vulkan frontend/backend, and staged loader boundary in [docs/android-native-loader-design.md](android-native-loader-design.md).

**Caveat:** these function symbols are directly linked into the smoke NRO. This remains a prototype, not Android's private `ANativeWindow` ABI, `libandroid.so`, Android Vulkan loader, JNI/ART, NativeActivity, or APK runtime.

## 0.10 — Native module loader input inventory (not yet implemented)

- [x] Separate package/ELF inspection, ELF execution/import shims, Android-facing Vulkan dispatch, NXVK, and managed runtime as distinct gates.
- [x] Specify conditional Android extension reporting and explicit rejection of unsupported Vulkan features.
- [ ] Build a self-authored ARM64 Android `.so` fixture in GitHub Actions and record its dependencies, undefined imports, symbol versions, and relocation types.
- [ ] Add host-side bounds-checked ELF inspection tests; do not execute parsed inputs.
- [ ] Select and implement only the loader/import subset required by the reviewed fixture.

**Design:** [docs/android-native-loader-design.md](android-native-loader-design.md). This design is not a loader implementation or APK support.

## 1 — APK inspection (host tool)

- [ ] Safely enumerate APK ZIP entries and reject malformed/path-traversal inputs.
- [ ] Report package name, declared min/target SDK (when present), ABI directories, native libraries, and assets.
- [ ] Add synthetic and self-authored fixtures; test corrupt and oversized inputs.
- [ ] Clearly distinguish metadata that can be parsed from behavior that is not implemented.

**Exit condition:** automated host tests run without a Switch.

## 2 — Define a runnable app subset

- [x] Add a small self-authored Unity Android reference project (`unity-smoke/`) with visible startup, JNI, asset, storage, input, and lifecycle probes.
- [ ] Build the fixture APK with Unity 2022.3 LTS Android modules and run it on an ARM64 Android device; save build/device logs.
- [ ] Inventory the exact DEX, JNI, framework, graphics, audio, and storage behavior it uses.
- [ ] Decide whether to investigate adapting an existing runtime or building a narrower runtime; record licensing and platform constraints before importing code.
- [ ] Write a prototype design and measurable compatibility criteria before committing to an execution architecture.

**Exit condition:** a reviewed design with a testable first execution path and no assumption that a shared-object loader is a full Android runtime. Checking in the fixture source alone does not complete this milestone.

## 3 — First end-to-end execution

- [ ] Implement the smallest agreed execution path on Horizon.
- [ ] Load the sample's package/assets from user-provided storage.
- [ ] Add explicit diagnostics for every unsupported required API.
- [ ] Test launch, input, lifecycle/exit, and repeat launch.

**Exit condition:** the self-authored sample runs repeatably on target hardware, and limitations are documented.

## 4 — Expand compatibility deliberately

- [ ] Track Android APIs and services by tested coverage.
- [ ] Add one API family at a time with tests.
- [ ] Add performance, memory pressure, and malformed APK tests.
- [ ] Only claim support for specific app classes or apps that have been tested.

No compatibility claim should be inferred solely from successful APK parsing or loading one native library.