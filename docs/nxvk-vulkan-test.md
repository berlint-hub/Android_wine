# Switch Vulkan/NXVK hardware smoke test

This test moves beyond the Horizon-only probe and asks whether the pinned NXVK driver can initialize Vulkan and present frames on the Switch. It is still **not** an Android runtime and does not launch Unity or an APK.

The GitHub Actions workflow `.github/workflows/build-nxvk-vulkan.yml` checks out NXVK at commit `1b7c38f6151b7cde61500d0a1e984b984587f706`, builds its upstream test programs, and uploads `.nro` files. The build is a substantial Mesa/NVK cross-build and can take a while. Dispatch it manually from **Actions → Build NXVK Vulkan smoke tests → Run workflow**, or use the artifact attached to a successful run.

## Tests

- `androidwine_vk_compute.nro` — upstream `nvk_smoke`: initializes NVK, submits a Vulkan buffer-fill command, and checks the result. It writes `sdmc:/nvk_smoke.log` and `sdmc:/nvk_mesa.log`.
- `androidwine_vk_triangle.nro` — upstream `nvk_tri`: builds a graphics pipeline with vertex/fragment shaders, draws an offscreen triangle, reads pixels back, and verifies the center is shaded while the corner remains clear. It writes `sdmc:/nvk_tri.log` and `sdmc:/nvk_mesa.log`; it is headless, so it does not show a triangle on screen.
- `androidwine_vk_texture.nro` — upstream `nvk_logo`: exercises `sampler2D`, descriptor sets, and buffer-to-image upload/readback. It writes `sdmc:/nvk_logo.log` and `sdmc:/nvk_mesa.log`; it is headless.
- `androidwine_vk_scene.nro` — upstream `nvk_scene`: renders a 3D cube with depth (ZETA) and checks readback. It writes `sdmc:/nvk_scene.log` and `sdmc:/nvk_mesa.log`; it is headless.
- `androidwine_vk_indexed.nro` — upstream `nvk_indexed`: validates indexed draws using both UINT16 and UINT32 index buffers. Log: `sdmc:/nvk_indexed.log`.
- `androidwine_vk_multi.nro` — upstream `nvk_multi`: exercises repeated draws, pipeline switches, and alpha blending. Log: `sdmc:/nvk_multi.log`.
- `androidwine_vk_textures.nro` — upstream `nvk_textures`: exercises mipmaps, sRGB decoding, and BC1 decompression. Log: `sdmc:/nvk_textures.log`.
- `androidwine_vk_cubemap.nro` — upstream `nvk_cubemap`: validates cube-compatible images and `samplerCube`. Log: `sdmc:/nvk_cubemap.log`.
- `androidwine_vk_present.nro` — upstream `nvk_vi_swapchain`: creates a Vulkan VI surface and swapchain, presents 300 changing-color frames, then exits (about five seconds at 60 FPS). It writes `sdmc:/nvk_vi_swapchain.log` and `sdmc:/nvk_mesa.log`. A visible changing-color screen is expected while it runs.

These validation apps are based on the NXVK upstream tests, rather than new AndroidWine Vulkan code. The driver and upstream Switch smoke sources are pinned in `SOURCE.txt` inside the artifact. The binaries statically link GPL-2.0-or-later NXVK code; the artifact includes the license and the exact public source revision. Keep the corresponding source notice when redistributing the NROs.

## Run on Switch

1. Download the artifact from the successful Actions run and extract it.
2. Copy the `.nro` test files you intend to run to `sdmc:/switch/` (or another Homebrew Menu folder). Keep their filenames.
3. Launch each test as a **game override** (hold **R** while launching a game, then start the NRO from Homebrew Menu). Applet mode has only shown about 403 MiB total process memory and may not be enough for the driver.
4. Run the eight headless tests in this order: `androidwine_vk_compute.nro`, `androidwine_vk_triangle.nro`, `androidwine_vk_texture.nro`, `androidwine_vk_scene.nro`, `androidwine_vk_indexed.nro`, `androidwine_vk_multi.nro`, `androidwine_vk_textures.nro`, and `androidwine_vk_cubemap.nro`. Each app log should end with its matching `=== ... PASSED ===` line. These tests validate compute, shaders, sampling/uploads, depth, indexed draws, blending, mipmaps/formats, and cubemaps via readback; they do not show graphics on screen.
5. Run `androidwine_vk_present.nro` last among the nine upstream tests. Note whether you see the changing colors (it auto-exits after 300 frames) and check `sdmc:/nvk_vi_swapchain.log` for `=== nvk_vi_swapchain PASSED ===`.
6. Send all nine app logs, the Mesa log from each run if available, whether the screen visibly changed color, and any crash/freeze details. Copy logs after each test because `nvk_mesa.log` is overwritten at startup.

The upstream present test automatically stops after 300 frames (about five seconds at 60 FPS) and exits; it does not require a Plus-button exit. A passing result establishes only the tested NVK compute/present path on this console and HOS configuration. It does not establish OpenGL ES/Zink, Unity, Android surface/JNI support, or APK compatibility.

## Experimental Android-to-Horizon surface adapter (separate test)

The same workflow builds `androidwine_vk_android_surface_bridge.nro` from the experimental source under `bridge/`. It is **not** one of the nine upstream tests above. The first two synthetic-token revisions passed on Switch with 60/60 presents. The current opaque-object/NDK-shaped-function revision also passed: the user log confirms function and lifecycle markers, a 1280×720 three-image VI swapchain, and 60/60 presents. CI push run [`39668507515`](https://github.com/berlint-hub/Android_wine/actions/runs/39668507515) and PR run [`39668520496`](https://github.com/berlint-hub/Android_wine/actions/runs/39668520496) built it; artifact `androidwine-nxvk-vulkan-4aed0fe071a377a81849a298b35ec431e2e6b760`. This verifies only the prototype function subset and direct Horizon surface path, not Android's private window ABI, Android loader/runtime or APK support. See [docs/android-horizon-bridge.md](android-horizon-bridge.md) for exact limitations.

For a rerun, launch the matching artifact NRO as a game override after the nine stable tests. It writes `sdmc:/androidwine_surface_bridge.log` and `sdmc:/nvk_mesa.log`. The expected log includes `anativewindow_functions=acquire,release,getWidth,getHeight,getFormat,setBuffersGeometry PASSED`, `window_lifecycle=owner-release, surface-retain, surface-destroy, stale-reject PASSED`, `presented_frames=60 expected=60`, and `=== ANDROIDWINE_SURFACE_BRIDGE_CONTRACT PASSED ===`. The supplied log does not report visual color confirmation.

## First hardware result

User-supplied logs show **all nine initial NXVK Vulkan ladder tests passed** on the Switch: `nvk_smoke`, `nvk_tri`, `nvk_logo`, `nvk_scene`, `nvk_indexed`, `nvk_multi`, `nvk_textures`, `nvk_cubemap`, and `nvk_vi_swapchain`. NVK identified `NVIDIA Tegra X1 (NVK gm20b)`, Vulkan API `1.4.354`. The results cover buffer compute/host mapping, shader pipeline and pixel readback, texture sampling, depth, UINT16/UINT32 indexed drawing, blending, seven-level sRGB mipmaps, BC1 sampling, cubemap sampling, and VI swapchain presentation of 300/300 frames at 1280×720. This is strong evidence that the tested NXVK Vulkan feature ladder works on this console/HOS setup. It does not establish Android loader/surface/JNI behavior, Unity startup, or APK compatibility.

## Expected outcomes

- **Compute + triangle pass:** Vulkan device setup, command submission, and a real NAK graphics-shader pipeline/readback work.
- **Texture + scene pass:** sampled images, descriptors, upload, and depth rendering/readback also work.
- **Indexed/multi/textures/cubemap pass:** common geometry, blending, mipmap/format, and cube-sampling features work.
- **Present passes:** the VI surface/swapchain accepts frames; confirm the screen was visibly updated as well.
- **Compute passes, triangle fails:** investigate NVK shader compilation/execution before attempting Unity.
- **Compute passes, present fails:** NVK device/command submission work, but the VI surface/swapchain path needs investigation.
- **Compute fails:** troubleshoot toolchain/HOS/driver initialization before attempting Unity.
- **NRO does not start or hangs:** report the specific app, resource mode, and matching logs. A missing pass line is not a pass.
