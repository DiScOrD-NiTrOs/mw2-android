# Changelog

All notable changes to the Modern Warfare 2 Android native recompilation project will be documented in this file.

## [v0.0.2-alpha] - 2026-10-18

### Highlights
- **Release tags with a suffix are now read.** `v0.0.2-alpha` was a tag the
  launcher's update check could not parse at all, so it answered "its
  description is not one" and no player was ever offered it — as every
  `v0.1.0-alpha.N` release before this one went unoffered.
- The apk says which release it is, instead of "1.0".
- Faster software decoding of block-compressed textures on Mali and Adreno
  (BC2 10.1 → 4.7 ms per 2048x2048 decode).

### Added
- **`launcher/version.h`**: release tags compared as semantic versioning orders
  them — `0.0.2-alpha < 0.0.2-beta < 0.0.2 < 0.0.3-alpha`, with `alpha.10` after
  `alpha.9` and not before it, and a tag that is not a version refused rather
  than ordered somewhere. `update::Look` uses it (`launcher/update.cpp`).
- **`tests/version_test.cpp`** (`ctest` name `version`): the order above, the
  project's own published tags, and the shapes that are not versions.
- **`tools/bc_bench.cpp`**, built as `bc-bench`: how long one 2048x2048
  software BC decode takes per format. The figures quoted in
  `runtime/gpu/vulkan/bc_decoder.cpp` come from it.
- **`MW2_VERSION_NAME`** (and `MW2_VERSION_CODE`) in `android/app/build.gradle.kts`:
  the release the apk was built from, taken from the tag by `build-apk.yml`.

### Changed
- **BC decoder, row at a time** (`runtime/gpu/vulkan/bc_decoder.cpp`): BC2, BC3
  and BC5 walked all sixteen pixels of a block with a computed x, y and shift;
  they now take a row from a word already loaded, with the block's masked
  colours and lifted alphas made once. On a 2048x2048 decode (best of 12,
  `bc-bench`): BC2 10.1 → 4.7 ms, BC3 7.1 → 6.4 ms, BC5 6.7 → 6.4 ms, BC1
  unchanged at 3.8 ms. The output is byte for byte what it was —
  `tests/bc_decoder_test.cpp` compares it against a decoder written from the
  formats themselves.
- **`versionCode` 15 → 16**, so Android installs this over v0.0.2.
- **`.github/workflows/release.yml`**: a tag that is not a version now fails the
  run before anything is published — a release tag the launcher cannot read is
  a release nobody is offered — and a tag with a suffix after its version is
  drafted as a pre-release, which is what keeps GitHub's "latest release" (the
  one the launcher asks for) on a version everyone can install.
- **`docs/building.md`**: the list of harnesses and `ctest`'s checks, which said
  three targets where there were four and now are five.

### Fixed
- **A build with no release tag no longer compares itself against the newest
  release.** The empty version parsed as no numbers, which every version sorts
  above, so an unreleased build was offered an update it should not have been
  (`update.h` says such a build does not update; now it does not).

### Notes
- GitHub's "latest release" — the endpoint the launcher reads — is never a
  pre-release, so publishing `v0.0.2-alpha` as a pre-release reaches players
  through the releases page and the apk, not through the in-launcher updater.
  That is the intended behaviour for an alpha; a pre-release build being
  *offered* pre-releases would need the list endpoint instead.

---

## [v0.0.2] - 2026-10-13

### Highlights
- Comprehensive fixes for Qualcomm Snapdragon (Adreno) chips.
- Compatibility floor lowered to **Vulkan 1.0**.
- ARM64 CPU thread affinity for Prime (Cortex-X) cores.
- TBDR mobile tile rendering optimizations.
- Virtual Filesystem (VFS) mod directory overlay (`mods/`).
- Ingestion security hardening for disc extraction and archives.

### Added
- **ARM64 CPU Affinity**: Pinned main game loop to the Prime Core (Cortex-X), command recorder to Performance Cores, and background worker to Efficiency Cores via Bionic `sched_setaffinity` (`runtime/platform.cpp`).
- **Mod Directory Overlay**: Added support for offline Bot Warfare GSC mods and loose assets located in `MW2_MOD_DIR` or `<GameRoot>/mods/` (`runtime/kernel/files.cpp`).
- **Pillar Architecture Documentation**: Created technical specifications for CPU affinity, Vulkan mobile pipeline, memory budgeting, and bots/netcode in `docs/pillars/`.
- **In-Game FOV Control**: Merged upstream FOV slider support and camera adjustments.

### Fixed
- **Snapdragon / Adreno BC Texture Bug**:
  - Automatically routes BC1–BC5 textures through SIMD software decoding on Qualcomm GPUs (`vendorID == 0x5143`) to avoid hardware 1-bit alpha sampling defects.
  - Representation-safe texture uploads and restaging in `runtime/gpu/vulkan/texture_cache.cpp`: decompression failures are explicitly caught and reported, preventing black terrain, flashing vegetation, and black skybox artifacts.
- **Vulkan 1.0 Support**:
  - Manifest hardware requirement lowered to `0x400000` (`android/app/src/main/AndroidManifest.xml`).
  - Dynamic capability detection and fallback to classic Vulkan 1.0 render passes and negative viewport extensions.
- **TBDR Mobile RenderPass Optimization**:
  - Configured `VK_ATTACHMENT_STORE_OP_DONT_CARE` on stencil and depth targets in `runtime/gpu/vulkan/renderer_targets.cpp`, eliminating tile memory spilling.
- **Archive Extraction Hardening**:
  - Fixed order-of-operations bug in `tools/fetch_asset.py` where temporary file was deleted before reading header bytes.
  - Implemented zip-bomb bounds (50,000 entries and 15 GiB limit).
- **XDVDFS Disc Extraction Hardening**:
  - Added sector bounds validation against total image size (`tools/xdvdfs.py`).
  - Added directory cycle detection and recursion depth limits.
  - Enforced destination path containment using `os.path.realpath` to prevent path traversal.
  - Added truncated read validation.
- **CI & Release Determinism**:
  - Made manual APK builds artifact-only by default and enforced explicit release tag validation (`.github/workflows/build-apk.yml`).

---

## [v0.0.1] - 2026-10-10

### Added
- Initial release of the native Modern Warfare 2 recompilation port for Android ARM64 (`com.mw2.recomp.sp`).
- XenonRecomp static PowerPC recompiler generating 64-bit ARM machine code (`libmw2.so`).
- Vulkan renderer backend supporting mobile TBDR architectures.
- Low-latency AAudio native sound sink at 48 kHz stereo.
- Integrated Android launcher with on-screen touch controls, gamepad support, and XDVDFS ISO installer.
- Custom GPU driver installer (`libadrenotools`) in `DriverStore.kt`.
