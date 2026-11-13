# FontForgeGUI Source & Architecture (`src/`)

This document describes the internal directory structure, threading model, FontForge integration, and key implementation details of FontForgeGUI. For user-facing overview, screenshots, and quick-start build instructions, see the root [README.md](../README.md).

---

## Directory Layout

- `backend/` — FontForge C API wrapper, thread boundary, and immutable DTOs:
  - `FontTypes.hpp`: Plain Qt/C++ data transfer structs (`FontSummary`, `GlyphSummary`, `SvgImportOptions`, `BatchImportJob`, `FontMergeResult`, `CreateGlyphRequest`, `MoveGlyphsRequest`, etc.) and placement helpers (`computeIconPlacement`).
  - `FontForgeBridge.hpp/.cpp`: Encapsulates all `libfontforge` C headers (`fontforge.h`, `splinefont.h`, `splineutil.h`, `encoding.h`, `namelist.h`, etc.) strictly inside `.cpp` so no FontForge macros or globals leak into Qt UI headers. Implements `ui_interface` hooks for log/progress forwarding.
  - `FontWorker.hpp/.cpp`: Owns a `FontForgeBridge` instance on a dedicated `QThread` and exposes queued Qt slots/signals.
- `model/` — Pure Qt/C++ domain logic (no `QWidget` dependencies; linked into `ffgui_core` for headless CLI & CTest targets):
  - `SvgSanitizer.hpp/.cpp`: Multi-pass DOM sanitizer (`QDomDocument`) that strips editor metadata/noise, resolves inherited CSS/presentation attributes, detects/filters colors, separates fills (`solidSvg`), strokes (`strokeSvg`), and white foreground cutouts (`cutoutSvg`), and pads tiny-path checks by `strokeWidth` for stroked icons.
  - `BitmapTracer.hpp/.cpp`: In-memory raster-to-SVG tracer using Alpha/Luminance, perceptual Color Distance keying, Dominant Color Palette quantization, or Sobel Edge Gradient filtering, followed by Marching Squares contour/hole extraction, Ramer–Douglas–Peucker simplification, and corner-preserving cubic Bézier smoothing.
  - `BatchImportDefinition.hpp/.cpp`: JSON job serialization/deserialization for batch imports.
  - `FontSettingsUtils.hpp`: Centralized `QSettings` keys and helpers (`lastFontDir`, `defaultImportOptions`, `toolbarDisplayMode`).
  - `GlyphClipboardUtils.hpp/.cpp`: Clipboard MIME handling (`application/x-fontforgegui-glyphs+json`, `image/svg+xml`, `text/plain`).
  - `GlyphMetadataUtils.hpp/.cpp`: Shared Unicode code-point parsing, glyph name validation, and `uniXXXX` / `uXXXXX` name derivation.
  - `UnicodeRangeFilter.hpp/.cpp` & `CustomGlyphAreaConfig.hpp/.cpp`: Range parser (`Basic Latin` through `SPUA-A`/`SPUA-B`) and `.ffgui.json` sidecar persistence.
  - `GlyphListModel.hpp/.cpp`: `QAbstractListModel` providing virtualized glyph grid filtering, search, and incremental updates.
- `ui/` — Qt 6 Widgets presentation layer (`FontForgeGUI` executable):
  - `MainWindow.hpp/.cpp`: Unified `QGridLayout` filter/area bar, main toolbar with configurable `ToolbarDisplayMode`, dock panels (`ValidationPanel`, FontForge log), and `GlyphGridView` / `GlyphInspectorPanel` splitter.
  - `IconUtils.hpp/.cpp`: Loads monochrome `currentColor` SVGs from `resources/resources.qrc` and tints them via `QSvgRenderer` (`Qt6::Svg`) to active/disabled `QPalette::ButtonText` colors.
  - `ImportSvgDialog.hpp/.cpp`: Interactive side-by-side preview (`Original` vs. `GlyphPlacementPreviewWidget`) with drag-to-offset, wheel-to-scale, color swatches, and SVG/PNG filter controls.
  - `BatchImportDialog.hpp/.cpp`, `ImportFilterSettingsDialog.hpp/.cpp`, `ImportFontDialog.hpp/.cpp`, `MoveGlyphsDialog.hpp/.cpp`, `CreateGlyphDialog.hpp/.cpp`, `NewFontDialog.hpp/.cpp`, `ExportFontDialog.hpp/.cpp`, `FontMetadataDialog.hpp/.cpp`, `SettingsDialog.hpp/.cpp`.
- `resources/` — `resources.qrc` and 20 monochrome 24×24 SVG icons under `resources/icons/`.
- `tests/` — Automated CTest executables (`test_backend_smoke`, `test_unicode_range`, `test_v02_metadata_validation`, `test_v03_svg_import`, `test_v04_features`, `test_v05_features`).

---

## Key Implementation Details

### 1. Thread Safety & `libfontforge` Isolation
- `libfontforge` relies on global C state and is not thread-safe. All FontForge calls execute exclusively inside `FontForgeBridge` on the dedicated `FontWorker` `QThread`.
- The UI never holds raw `SplineFont*` or `SplineChar*` pointers; it consumes immutable `FontDocumentSnapshot` / `GlyphSummary` structs containing pre-extracted `QPainterPath` outlines (`Qt::WindingFill`, Y-up font coordinates flipped on draw).

### 2. Multi-Pass SVG Import (`SvgSanitizer` + `FontForgeBridge`)
- `SvgSanitizer` splits geometry into up to three clean XML streams:
  1. `solidSvg` (filled foreground shapes)
  2. `strokeSvg` (stroked shapes; expanded by FontForge's `SplinePointListInterpretSVG` -> `SplineSetStroke` and normalized with `SplineSetsCorrect`)
  3. `cutoutSvg` (`#ffffff` foreground holes subtracted after `SplineSetRemoveOverlap(..., over_remove)` via CCW winding reversal)
- For stroke-only SVGs (`solidSvg` empty, `strokeSvg` non-empty), callers must **not** fall back from `solidSvg` to `cleanedSvg`; otherwise FontForge imports the strokes twice with opposite windings, cancelling the fill.

### 3. Icons (`Qt6::Svg` + `IconUtils`)
- Embedded SVGs use `stroke="currentColor"`. Because Qt's `qsvgicon` plugin does not resolve `currentColor` against `QPalette`, `IconUtils` substitutes `currentColor` with `QPalette::ButtonText` (Active and Disabled) before rasterizing via `QSvgRenderer` into multi-size `QIcon` pixmaps (16/20/24/32 px). This avoids runtime `iconengines/qsvgicon` plugin issues on Windows and works across light/dark themes.

### 4. Build & Testing Notes
- FontForge is built as a headless shared subproject (`ENABLE_GUI=OFF`, `BUILD_SHARED_LIBS=ON`, `ENABLE_PYTHON_SCRIPTING=OFF`) from `FONTFORGE_SOURCE_DIR` (default `../../FontForge` relative to `src/`).
- Tests use a custom `CHECK(expr)` macro instead of standard `assert(...)` because `RelWithDebInfo` defines `NDEBUG`.
- Headless test runs require `QT_QPA_PLATFORM=offscreen`.
