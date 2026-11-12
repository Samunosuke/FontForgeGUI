# FontForgeGUI

Modern cross-platform Qt 6 Widgets GUI for FontForge, built directly on headless `libfontforge` (`ENABLE_GUI=OFF`).

---

## Key Features

- **Font Creation & Editing**:
  - **Create New Font (`Ctrl+N`)**: Initialize a brand new font in-memory with customizable metadata, Ascent/Descent, and EM size without touching existing files.
  - **Save In-Place (`Ctrl+S`) & Save As (`Ctrl+Shift+S`)**: Persistent document path binding, dirty indicator (`*`), and last font directory memory.
  - **Font & Glyph Metadata Editor**: Modify Font Family, PostScript Name, Weights, Copyright, and full SFNT `name` table. Edit glyph Unicode code points, names, and alternate unicodes.
  - **Validation Dock**: Real-time font integrity checks via FontForge's `SFValidate` with clickable issue navigation.

- **Glyph Grid & Navigation**:
  - Fast vector-rendered glyph grid with customizable cell zoom.
  - Extended multi-selection (Shift/Ctrl + Click).
  - Unicode Range Filtering (Basic Latin, Latin-1, PUA, and **Supplementary Private Use Areas SPUA-A `U+F0000–U+FFFFD` & SPUA-B `U+100000–U+10FFFD`**).
  - Custom Glyph Areas with `.ffgui.json` sidecar persistence.

- **Glyph Relocation ("Move Glyphs" `Ctrl+Shift+M`)**:
  - Move multiple selected glyphs to a new starting code point (e.g. `U+F0000` or `0xF0000`).
  - Debounced real-time collision detection against existing glyphs with overwrite/abort protection and color-coded preview.

- **Advanced Vector & SVG Import**:
  - Interactive SVG Sanitizer dialog: Strips Inkscape/Sodipodi metadata and hidden shapes.
  - Color detection and exclusion filters.
  - Automatic `#ffffff` cutout path extraction (creates clean interior holes).
  - Stroke-to-path expansion and tiny path despeckling.
  - Visual placement canvas: Pan, zoom, and drag icon offsets with live FontForge baseline, ascent, and descent metrics.

- **Extended PNG & Bitmap Vector Tracing (`v0.4` – `v0.5.2`)**:
  - Built-in in-memory contour tracer (`BitmapTracer`): Marching Squares directed edge extraction, Ramer–Douglas–Peucker simplification, and corner-preserving cubic Bézier smoothing.
  - **Four Binarization Modes**:
    1. **Alpha / Luminance (Standard)**: Transparent alpha masks with auto-fallback to luminance.
    2. **Color Distance (Background Keying)**: Auto-detects background color from corner pixels; uses perceptual color distance ($\Delta C = \sqrt{2 \Delta R^2 + 4 \Delta G^2 + 3 \Delta B^2}$) to cleanly isolate multicolored icons (yellow, cyan, red, blue) from white/dark backgrounds.
    3. **Dominant Color Palette**: Fast 8-bit color quantization into 4–8 dominant colors with clickable toggle swatches to choose which colors form the foreground vector.
    4. **Edge Gradient (Sobel Filter)**: 3x3 multi-channel gradient filter to extract contour lines between adjacent color regions.
  - Persistent application-wide import preferences in `QSettings` (`Edit -> Import Preferences…`).

- **Batch Import & Automation (GUI & CLI)**:
  - **Batch Import Dialog (`Ctrl+Shift+B`)**: Batch import dozens or hundreds of SVGs or PNGs from folders or files starting at any Unicode code point.
  - **Structured JSON Job Definitions**: Import and export batch definitions including custom per-glyph options or global defaults.
  - **Headless CLI Execution**: Run full batch imports or conversions from the terminal without opening a GUI window.

---

## Architecture

- `backend/` — Thread-safe `FontWorker` (dedicated `QThread`) wrapping `FontForgeBridge` (`libfontforge` C API + `ui_interface` hooks) and immutable DTOs (`FontTypes.hpp`): font load/export, new font creation, glyph relocation, validation (`SFValidate`), and batch imports.
- `model/` — 
  - `BitmapTracer`: In-memory PNG contour tracer with alpha, luminance, color-distance keying, dominant palette quantization, and Sobel edge detection.
  - `BatchImportDefinition`: JSON serialization and deserialization for batch jobs.
  - `FontSettingsUtils`: Centralized parameter key constants, shared JSON serializers, and persistent `QSettings`.
  - `SvgSanitizer`: SVG vector cleaner and color separator.
  - `UnicodeRangeFilter` & `CustomGlyphAreaConfig`: Sidecar persistence and Unicode range definitions (including SPUA-A/B).
  - `GlyphListModel`: `QAbstractListModel` virtualized data provider.
- `ui/` — 
  - `MainWindow`: Main ribbon, menus, zoom controls, and dock panels.
  - `GlyphGridView` & `GlyphGridDelegate`: Vector cards with Drag & Drop font loading.
  - `NewFontDialog` & `CreateGlyphDialog`: Creation dialogs for fonts and glyphs.
  - `MoveGlyphsDialog`: Debounced multi-glyph code point relocation dialog.
  - `ImportSvgDialog`: Side-by-side interactive preview for SVG and PNG raster vectorization.
  - `BatchImportDialog` & `ImportFilterSettingsDialog`: Batch automation and global filter preference management.
  - `ExportFontDialog`, `FontMetadataDialog`, and `ValidationPanel`.
- `tests/` — Automated CTest suite (6 test executables covering smoke tests, Unicode ranges, metadata validation, SVG import, bitmap tracing, and batch features).

---

## Command Line Usage

FontForgeGUI supports opening fonts directly or executing batch jobs:

```bash
# 1. Open an existing font in the GUI
FontForgeGUI myfont.ttf

# 2. Run a batch import job headless into a new font
FontForgeGUI --batch-import icons.json --output myicons.ttf --headless

# 3. Batch import images starting at U+F0000 into an existing or new font
FontForgeGUI --input base.ttf --output output.ttf --start-codepoint U+F0000 --images ./icons/*.png --headless

# 4. Open the GUI with batch parameters preloaded for confirmation
FontForgeGUI --batch-import icons.json --gui
```

---

## Build & Run (Ubuntu / Linux)

Install build dependencies using [`scripts/install_dependencies.sh`](../scripts/install_dependencies.sh):

```bash
./scripts/install_dependencies.sh
```

FontForge is referenced from the sibling repository `../FontForge` by default (override with `-DFONTFORGE_SOURCE_DIR=/path/to/FontForge`).

```bash
cmake -G Ninja -S src -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/FontForgeGUI
```

---

## Build & Run (Windows / MSYS2 UCRT64)

1. Install [MSYS2](https://www.msys2.org/) (default path `C:\msys64`).
2. Run [`scripts/build_windows.ps1`](../scripts/build_windows.ps1) in PowerShell:

```powershell
# First time: install MSYS2 UCRT64 packages, configure, build, and run CTest
.\scripts\build_windows.ps1 -InstallDeps

# Build and execute all tests
.\scripts\build_windows.ps1

# Create standalone dist-win\ bundle (FontForgeGUI.exe + Qt6/MinGW/FontForge DLLs)
.\scripts\build_windows.ps1 -Package
```
