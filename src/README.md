# FontForgeGUI

Modern cross-platform Qt 6 Widgets GUI for FontForge, built directly on headless `libfontforge` (`ENABLE_GUI=OFF`).

## Architecture (v0.3)

- `backend/` — Thread-safe `FontWorker` (dedicated `QThread`) wrapping `FontForgeBridge` (`libfontforge` C API + `ui_interface` hooks) and immutable DTOs (`FontTypes.hpp`): font load/export, glyph creation (`createGlyph`), glyph & font metadata edits, validation (`SFValidate`), and multi-pass SVG import (fills, strokes, and white-foreground cutouts).
- `model/` — `UnicodeRangeFilter`, `CustomGlyphAreaConfig` (`.ffgui.json` sidecar persistence), `GlyphListModel` (`QAbstractListModel`), `GlyphMetadataUtils` (shared code point & glyph name parsing/validation), and `SvgSanitizer` (Inkscape/Sodipodi metadata & noise stripping, color detection/exclusion, white-as-cutout extraction, stroke pass separation).
- `ui/` — `MainWindow`, `GlyphGridView` (with empty-state font Drag & Drop), `GlyphGridDelegate` (vector `QPainterPath` preview cards), `GlyphInspectorPanel` (inline metadata editor + glyph preview), `CreateGlyphDialog` (new glyph with Unicode/Name/Alt Unicodes/Advance Width + optional SVG import follow-up), `ImportSvgDialog` (side-by-side Original vs. Filtered SVG preview, SVG Drag & Drop, persistent last directory), `ExportFontDialog`, `FontMetadataDialog` (including SFNT `name` table), and `ValidationPanel`.
- `tests/` — CTest suite (`backend_smoke`, `unicode_range`, `v02_metadata_validation`, `v03_svg_import`).

## Implemented Highlights

- **v0.1:** Font loading (plus Drag & Drop onto empty glyph grid), virtualized vector glyph grid, Custom Glyph Areas with `.ffgui.json` sidecar persistence, Find Glyph (Unicode, Name, GID), and font export (`SFDWrite` / `GenerateScript`).
- **v0.2:** Inline Glyph Metadata Editor (`Name`, `Unicode`, `Alt Unicodes`), Font Metadata & SFNT `name` table editor, and Font Validation dock with clickable issue navigation.
- **v0.3:** Create New Glyph dialog (`Ctrl+N`), interactive SVG Sanitizer & Import dialog (color exclusion, `#ffffff` foreground cutout subtraction, stroke expansion, persistent last directory, SVG Drag & Drop).

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

You can also pass a font file on the command line (or drop one onto the empty glyph grid):

```bash
./build/FontForgeGUI ../FontForge/tests/fonts/Ambrosia.sfd
```

## Build & Run (Windows / MSYS2 UCRT64)

1. Install [MSYS2](https://www.msys2.org/) (default path `C:\msys64`).
2. Run [`scripts/build_windows.ps1`](../scripts/build_windows.ps1) in PowerShell:

```powershell
# First time: install MSYS2 UCRT64 packages, configure, build, and run CTest
.\scripts\build_windows.ps1 -InstallDeps

# Create standalone dist-win\ bundle (FontForgeGUI.exe + Qt6/MinGW/FontForge DLLs)
.\scripts\build_windows.ps1 -Package
```
