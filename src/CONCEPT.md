# FontForgeGUI concept

FontForgeGUI is a GUI application for FontForge. It provides a user-friendly interface for editing fonts.

It uses the FontForge core (`libfontforge`, built headless with `ENABLE_GUI=OFF`) from an external forked repository (`../FontForge`, configurable via `FONTFORGE_SOURCE_DIR`) as its backend and provides a modern Qt 6 desktop GUI on top.

## Architecture & Decisions

- **License:** GPL-3.0-or-later (required by linking `libfontforge`).
- **Backend Integration:** Direct linking against headless `libfontforge` via `FontForgeBridge` + `ui_interface` hooks instead of extracting/copying parser code.
- **Thread Safety:** `libfontforge` relies on global C state and is not thread-safe; all FontForge calls are serialized on a dedicated `FontWorker` thread while the UI consumes immutable snapshots (`FontDocumentSnapshot`) and `QAbstractListModel` (`GlyphListModel`).
- **Custom Processing Outside FontForge (planned for v0.3):** SVG pre-filtering/sanitization before passing paths to FontForge (`SplinePointListInterpretSVG`), and interactive bitmap-to-vector preview before tracing.

## Features

- Font editing
- Font exporting
- Font importing
- Font validation
- Font preview
- Import svg file and convert to font glyph
- Import icon image and convert to font glyph
- Glyph Metadata Editor with Glyph Preview 
- Font Metadata Editor 
- Find Glyph by Unicode, Name, Position
- Custom Glyph Area - Range of glyphs which will be displayed and can be easily selected for edit them
    - Basically implemented as a range/s of Unicode code points (it filters all glyphs in the font)
    - It can be changed dynamically
    - It will be saved in the font file or in separate file if font file does not support it (`.ffgui.json` sidecar implemented; optional SFD embedding later)

## UI

The UI is designed to be simple and easy to use.

- Cross Platform: Linux first (Ubuntu 26.04), Windows later (cross-compile)
- **C++20 + Qt 6 Widgets** (`QListView` + `QPainterPath` delegate for grid preview, `QGraphicsView` planned for v0.4 outline editor; QML was evaluated and intentionally rejected in favor of Widgets for complex desktop tables/dialogs and vector editing)

### Implementation Notes & Open Questions
- [x] **Font importing (`v0.1`):** `LoadSplineFont` + `FVAppend(_FontViewCreate(...))` via `FontForgeBridge`
- [x] **Font exporting (`v0.1`):** `GenerateScript` (OTF, TTF, WOFF, WOFF2, UFO, PFB, SVG) and `SFDWrite` (`.sfd`)
- [x] **Font preview & Find Glyph (`v0.1`):** `SplineSet`/`RefChar` -> `QPainterPath` rendered in virtualized `GlyphListModel` + `GlyphInspectorPanel`, filterable by Unicode, Name, or GID
- [x] **Font validation (`v0.2`):** `SFValidate` + `validation_state` flags exposed via `FontForgeBridge` and interactive `ValidationPanel` dock
- [x] **Glyph & Font metadata editors (`v0.2`):** Edit `SplineChar` (`name`, `unicodeenc`, `altuni`) inline in `GlyphInspectorPanel` and `SplineFont` (`fontname`, `familyname`, `fullname`, `weight`, `version`, `copyright`, `ascent`, `descent`, `italicangle`, `upos`, `uwidth`, SFNT `name` table) via `FontMetadataDialog` on `FontWorker`
- [ ] **SVG filter & import (`v0.3`):** Pre-filter/normalize Inkscape SVGs (strip unnecessary colors/groups/metadata) before calling FontForge SVG import
- [ ] **Icon image import & trace (`v0.3`):** Grayscale/threshold preview + `potrace` / tracing pipeline into glyph contours
- [ ] **Font editing / outline editor (`v0.4`):** Interactive `QGraphicsView` canvas mapping Bézier anchors/control handles directly to `SplinePoint`/`SplineSet` mutations on `FontWorker`

### Milestones

#### v0.1 (Implemented)
- [x] Import/Load Font (supported by FontForge)
- [x] Preview Glyph(s) - in Custom Glyph Area (with `.ffgui.json` sidecar persistence & Find Glyph by Unicode/Name/GID)
- [x] Export Font (supported by FontForge)

#### v0.2 (Implemented)
- [x] Edit Glyph Metadata (only name and unicodes)
- [x] Edit Font Metadata
- [x] Font Validation

#### v0.3
- [ ] Import SVG filter to reduce unnecessary geometries or colors information
- [ ] Import Glyph geometry from SVG (created by Inkscape or similar program)

#### v0.4
- [ ] Edit Glyph outline (simple and easy to use editor)
