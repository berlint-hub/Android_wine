# Switch Vulkan/NXVK hardware smoke test

This test moves beyond the Horizon-only probe and asks whether the pinned NXVK driver can initialize Vulkan and present frames on the Switch. It is still **not** an Android runtime and does not launch Unity or an APK.

The GitHub Actions workflow `.github/workflows/build-nxvk-vulkan.yml` checks out NXVK at commit `1b7c38f6151b7cde61500d0a1e984b984587f706`, builds its upstream test programs, and uploads two `.nro` files. This is a substantial Mesa/NVK cross-build; the workflow can take a while. Dispatch it manually from **Actions → Build NXVK Vulkan smoke tests → Run workflow**, or use the artifact attached to a successful run.

## Tests

- `androidwine_vk_compute.nro` — upstream `nvk_smoke`: initializes NVK, submits a Vulkan buffer-fill command, and checks the result. It writes `sdmc:/nvk_smoke.log` and `sdmc:/nvk_mesa.log`.
- `androidwine_vk_present.nro` — upstream `nvk_vi_swapchain`: creates a Vulkan VI surface and swapchain, presents 300 changing-color frames, then exits. It writes `sdmc:/nvk_vi_swapchain.log` and `sdmc:/nvk_mesa.log`. A visible changing-color screen is expected for several seconds.

These validation apps are based on the NXVK upstream tests, rather than new AndroidWine Vulkan code. The driver and upstream Switch smoke sources are pinned in `SOURCE.txt` inside the artifact. The binaries statically link GPL-2.0-or-later NXVK code; the artifact includes the license and the exact public source revision. Keep the corresponding source notice when redistributing the NROs.

## Run on Switch

1. Download the artifact from the successful Actions run and extract it.
2. Copy both `.nro` files to `sdmc:/switch/` (or another Homebrew Menu folder). Keep the filenames.
3. Launch each test as a **game override** (hold **R** while launching a game, then start the NRO from Homebrew Menu). Applet mode has only shown about 403 MiB total process memory and may not be enough for the driver.
4. Run `androidwine_vk_compute.nro` first. Check `sdmc:/nvk_smoke.log` for `=== nvk_smoke PASSED ===` and `sdmc:/nvk_mesa.log` for driver diagnostics.
5. Then run `androidwine_vk_present.nro`. Confirm the screen shows changing colors and that `sdmc:/nvk_vi_swapchain.log` ends with `=== nvk_vi_swapchain PASSED ===`.
6. Send both app logs, the Mesa log, whether the screen visibly changed color, and any crash/freeze details. Copy logs after each test because the Mesa log is overwritten at startup.

The upstream present test automatically stops after 300 frames (about five seconds at 60 FPS) and exits; it does not require a Plus-button exit. A passing result establishes only the tested NVK compute/present path on this console and HOS configuration. It does not establish OpenGL ES/Zink, Unity, Android surface/JNI support, or APK compatibility.

## Expected outcomes

- **Both pass:** we have a Vulkan foundation; next inspect the exact Unity application's Vulkan requirements and design the Android-to-Horizon surface/loader bridge.
- **Compute passes, present fails:** NVK device/command submission work, but the VI surface/swapchain path needs investigation.
- **Compute fails:** troubleshoot toolchain/HOS/driver initialization before attempting Unity.
- **NRO does not start or hangs:** report the specific app, resource mode, and matching logs. A missing pass line is not a pass.
