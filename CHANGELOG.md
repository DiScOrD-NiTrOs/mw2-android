# Changelog

All notable changes to the Modern Warfare 2 Android native recompilation project will be documented in this file.

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
