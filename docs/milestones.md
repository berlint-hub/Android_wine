# Milestones

Milestones are gates, not promises of universal compatibility. Each should have a reproducible test and a documented unsupported-feature list.

## 0 — Build and hardware proof

- [ ] Select and document the devkitPro/libnx toolchain version.
- [ ] Build a minimal `.nro` from a clean checkout.
- [ ] Run it on a Switch with homebrew support; show a screen and read controller input.
- [ ] Add logging and a clean exit path.

**Exit condition:** a reproducible build plus a recorded manual test procedure. This proves only the homebrew shell, not Android compatibility.

## 1 — APK inspection (host tool)

- [ ] Safely enumerate APK ZIP entries and reject malformed/path-traversal inputs.
- [ ] Report package name, declared min/target SDK (when present), ABI directories, native libraries, and assets.
- [ ] Add synthetic and self-authored fixtures; test corrupt and oversized inputs.
- [ ] Clearly distinguish metadata that can be parsed from behavior that is not implemented.

**Exit condition:** automated host tests run without a Switch.

## 2 — Define a runnable app subset

- [ ] Build a small self-authored sample APK.
- [ ] Inventory the exact DEX, JNI, framework, graphics, audio, and storage behavior it uses.
- [ ] Decide whether to investigate adapting an existing runtime or building a narrower runtime; record licensing and platform constraints before importing code.
- [ ] Write a prototype design and measurable compatibility criteria before committing to an execution architecture.

**Exit condition:** a reviewed design with a testable first execution path and no assumption that a shared-object loader is a full Android runtime.

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