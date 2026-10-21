# Switch Vulkan/NXVK hardware smoke test

This test moves beyond the Horizon-only probe and asks whether the pinned NXVK driver can initialize Vulkan and present frames on the Switch. It is still **not** an Android runtime and does not launch Unity or an APK.

The GitHub Actions workflow `.github/workflows/build-nxvk-vulkan.yml` checks out NXVK at commit `1b7c38f6151b7cde61500d0a1e984b984587f706`, builds its upstream test programs, and uploads three `.nro` files. The current three-test artifact is `androidwine-nxvk-vulkan-76da3a07932e0c9e345923e72766868d89186fc0` from run `39595030407` (available for 14 days from the run). This is a substantial Mesa/NVK cross-build; the workflow can take a while. Dispatch it manually from **Actions → Build NXVK Vulkan smoke tests → Run workflow**, or use the artifact attached to a successful run.

## Tests

- `androidwine_vk_compute.nro` — upstream `nvk_smoke`: initializes NVK, submits a Vulkan buffer-fill command, and checks the result. It writes `sdmc:/nvk_smoke.log` and `sdmc:/nvk_mesa.log`.
- `androidwine_vk_triangle.nro` — upstream `nvk_tri`: builds a graphics pipeline with vertex/fragment shaders, draws an offscreen triangle, reads pixels back, and verifies the center is shaded while the corner remains clear. This is the first NAK shader execution check. It writes `sdmc:/nvk_tri.log` and `sdmc:/nvk_mesa.log`; it is headless, so it does not show a triangle on screen.
- `androidwine_vk_present.nro` — upstream `nvk_vi_swapchain`: creates a Vulkan VI surface and swapchain, presents 300 changing-color frames, then exits (about five seconds at 60 FPS). It writes `sdmc:/nvk_vi_swapchain.log` and `sdmc:/nvk_mesa.log`. A visible changing-color screen is expected while it runs.

These validation apps are based on the NXVK upstream tests, rather than new AndroidWine Vulkan code. The driver and upstream Switch smoke sources are pinned in `SOURCE.txt` inside the artifact. The binaries statically link GPL-2.0-or-later NXVK code; the artifact includes the license and the exact public source revision. Keep the corresponding source notice when redistributing the NROs.

## Run on Switch

1. Download the artifact from the successful Actions run and extract it.
2. Copy both `.nro` files to `sdmc:/switch/` (or another Homebrew Menu folder). Keep the filenames.
3. Launch each test as a **game override** (hold **R** while launching a game, then start the NRO from Homebrew Menu). Applet mode has only shown about 403 MiB total process memory and may not be enough for the driver.
4. Run `androidwine_vk_compute.nro` first. Check `sdmc:/nvk_smoke.log` for `=== nvk_smoke PASSED ===` and save `sdmc:/nvk_mesa.log` for driver diagnostics.
5. Run `androidwine_vk_triangle.nro`. Check `sdmc:/nvk_tri.log` for `=== nvk_tri PASSED ===`; this test exercises NAK shaders offscreen.
6. Run `androidwine_vk_present.nro`. Note whether you see the changing colors (it auto-exits after 300 frames) and check `sdmc:/nvk_vi_swapchain.log` for `=== nvk_vi_swapchain PASSED ===`.
7. Send all three app logs, the Mesa log from each run if available, whether the screen visibly changed color, and any crash/freeze details. Copy logs after each test because `nvk_mesa.log` is overwritten at startup.

The upstream present test automatically stops after 300 frames (about five seconds at 60 FPS) and exits; it does not require a Plus-button exit. A passing result establishes only the tested NVK compute/present path on this console and HOS configuration. It does not establish OpenGL ES/Zink, Unity, Android surface/JNI support, or APK compatibility.

## First hardware result

User-supplied logs report `nvk_smoke PASSED` and `nvk_vi_swapchain PASSED` on the Switch. NVK identified `NVIDIA Tegra X1 (NVK gm20b)`, Vulkan API `1.4.354`; the compute test verified all 1,024 buffer words, and the WSI test created a 1280×720 surface and presented 300/300 frames. The user saw a brief color flicker. This confirms the logs and the Vulkan API present path; the next artifact adds the separate offscreen triangle/NAK shader check. The duration/visibility reported by the user is not a calibrated display test.

## Expected outcomes

- **Compute + triangle pass:** Vulkan device setup, command submission, and a real NAK graphics-shader pipeline/readback work.
- **Present passes:** the VI surface/swapchain accepts frames; confirm the screen was visibly updated as well.
- **Compute passes, triangle fails:** investigate NVK shader compilation/execution before attempting Unity.
- **Compute passes, present fails:** NVK device/command submission work, but the VI surface/swapchain path needs investigation.
- **Compute fails:** troubleshoot toolchain/HOS/driver initialization before attempting Unity.
- **NRO does not start or hangs:** report the specific app, resource mode, and matching logs. A missing pass line is not a pass.
