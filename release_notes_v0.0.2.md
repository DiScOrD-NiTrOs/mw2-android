# Modern Warfare 2 Android — Release v0.0.2 (Snapdragon & Vulkan 1.0 Update)

Version 0.0.2 brings comprehensive fixes for Qualcomm Snapdragon (Adreno) chips, lowers the hardware compatibility floor to **Vulkan 1.0**, introduces ARM64 Prime-core CPU thread affinity, implements TBDR mobile tile-rendering optimizations, and adds an offline mod directory overlay (`mods/`).

---

## 🎮 What's New in v0.0.2

### ⚡ Qualcomm Snapdragon (Adreno) Fixes
- **Black Terrain, Skybox & Flicker Resolution**:
  - Automatically identifies Adreno GPUs (`vendorID == 0x5143`) and routes BC compressed textures through software SIMD decoding to prevent Adreno's 1-bit alpha sampling hardware bug.
  - Representation-safe texture uploads and restaging: guarantees format consistency and validates payload byte size against image metadata, fixing black screens, flickering foliage, and missing geometry.
  - Option `MW2_FORCE_BC_HW=1` provided for users running custom clean Mesa Turnip drivers.
- **Snapdragon Prime-Core Thread Affinity**:
  - Automatically detects ARM big.LITTLE / tri-cluster architectures (Snapdragon 865 through 8 Gen 3).
  - Pinned main game loop to the Prime Core (Cortex-X1/X2/X3/X4), recorder/dispatch thread to Performance Cores, and background tasks to Efficiency Cores via Bionic `sched_setaffinity`.

### 📱 Vulkan 1.0 Mobile Support
- **Lowered Manifest Feature Requirement**:
  - Decreased `android.hardware.vulkan.version` requirement from `0x401000` (1.1) to `0x400000` (1.0).
  - Clean feature decoupling: dynamic rendering and extended dynamic state are dynamically negotiated, with automatic fallback to classic Vulkan 1.0 render passes and negative-viewport maintenance extensions.
- **TBDR Mobile RenderPass Optimization**:
  - Configured `VK_ATTACHMENT_STORE_OP_DONT_CARE` on stencil and temporary depth attachments to prevent GPU tile-memory thrashing on mobile SoCs.

### 🧩 VFS Mod Directory Overlay
- **Offline Bots & Loose Asset Support**:
  - Transparent virtual filesystem overlay via `MW2_MOD_DIR` or `<GameRoot>/mods/`.
  - Enables loading offline Bot Warfare GSC mods, custom scripts, localized language files, and loose assets without repacking `.ff` FastFiles.

### 🛡️ Hardened Tooling & Asset Ingestion
- **XDVDFS Disc Extractor (`tools/xdvdfs.py`)**:
  - Added strict sector bounds validation against total image size.
  - Prevented infinite recursion and directory loops.
  - Sanitized filenames against path traversal (`..`, slashes) and enforced destination containment.
  - Added truncated read detection.
- **Asset Fetching (`tools/fetch_asset.py`)**:
  - Fixed order-of-operations bug where files were unlinked prior to reading header bytes.
  - Added zip-bomb resource limits (capped total unpacked bytes to 15 GiB and maximum entry count to 50,000).

### 🔄 Upstream Engine Synchronization
- Merged upstream FOV slider support and frame arena write-combining optimization.
- Added network and security notices for LAN / matchmaking.

---

## 📦 Requirements

- **OS**: Android 8.0 (Oreo) or higher (API 26+)
- **Architecture**: `arm64-v8a` (64-bit ARM)
- **GPU**: Vulkan 1.0 or higher (Snapdragon / Adreno 600+, Dimensity / Mali-G57+, Exynos / Xclipse, Tensor)
- **RAM**: Minimum 4 GB, Recommended 6 GB+
- **Storage**: ~7 GB free internal storage for game data
- **Game Files**: An unmodified Xbox 360 Modern Warfare 2 disc image (`.iso`)

---

## 📥 Installation Guide

1. Download and install **`app-campaign-release.apk`** from the Assets below.
2. Transfer your legal Modern Warfare 2 Xbox 360 disc image (`.iso`) to your Android device (e.g. `Downloads` folder).
3. Open the MW2 launcher on your phone.
4. Tap **Select ISO / Install Game**, choose your `.iso` file, and wait for extraction.
5. Tap **Launch Campaign** and enjoy 60 FPS native Modern Warfare 2!
