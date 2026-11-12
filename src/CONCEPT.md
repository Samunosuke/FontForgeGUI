# FontForgeGUI concept

FontForgeGUI is a GUI application for FontForge. It provides a user-friendly interface for editing fonts.

It uses the FontForge core (`libfontforge`, built headless with `ENABLE_GUI=OFF`) from an external forked repository (`../FontForge`, configurable via `FONTFORGE_SOURCE_DIR`) as its backend and provides a modern Qt 6 desktop GUI on top.

## Architecture & Decisions

- **License:** GPL-3.0-or-later (required by linking `libfontforge`).
- **Backend Integration:** Direct linking against headless `libfontforge` via `FontForgeBridge` + `ui_interface` hooks instead of extracting/copying parser code.
- **Thread Safety:** `libfontforge` relies on global C state and is not thread-safe; all FontForge calls are serialized on a dedicated `FontWorker` thread while the UI consumes immutable snapshots (`FontDocumentSnapshot`) and `QAbstractListModel` (`GlyphListModel`).
- **Custom Processing Outside FontForge:** SVG pre-filtering/sanitization (`SvgSanitizer`) before passing geometry to FontForge (`SplinePointListInterpretSVG`), plus interactive bitmap-to-vector preview before tracing (planned/deferred).

## Features

- Font editing
- Font exporting
- Font importing (including Drag & Drop onto the empty glyph grid)
- Font validation
- Font preview
- Create New Glyph (Name, Unicode, Alt Unicodes, Advance Width, optional immediate SVG import)
- Import SVG file and convert to font glyph (with Drag & Drop, persistent last directory, color filter, and white cutout subtraction)
- Import icon image and convert to font glyph (planned/deferred)
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
- [x] **Font importing (`v0.1`):** `LoadSplineFont` + `FVAppend(_FontViewCreate(...))` via `FontForgeBridge`, plus Drag & Drop of font files into the empty `GlyphGridView`
- [x] **Font exporting (`v0.1`):** `GenerateScript` (OTF, TTF, WOFF, WOFF2, UFO, PFB, SVG) and `SFDWrite` (`.sfd`)
- [x] **Font preview & Find Glyph (`v0.1`):** `SplineSet`/`RefChar` -> `QPainterPath` rendered in virtualized `GlyphListModel` + `GlyphInspectorPanel`, filterable by Unicode, Name, or GID
- [x] **Font validation (`v0.2`):** `SFValidate` + `validation_state` flags exposed via `FontForgeBridge` and interactive `ValidationPanel` dock
- [x] **Glyph & Font metadata editors (`v0.2`):** Edit `SplineChar` (`name`, `unicodeenc`, `altuni`) inline in `GlyphInspectorPanel` (sharing validation/formatting via `GlyphMetadataUtils`) and `SplineFont` (`fontname`, `familyname`, `fullname`, `weight`, `version`, `copyright`, `ascent`, `descent`, `italicangle`, `upos`, `uwidth`, SFNT `name` table) via `FontMetadataDialog` on `FontWorker`
- [x] **Create New Glyph (`v0.3`):** `CreateGlyphDialog` (reusing `GlyphMetadataUtils`: Name, Unicode, Alt Unicodes, Advance Width, plus option to open `ImportSvgDialog` directly after creation) wired to `FontForgeBridge::createGlyph` (`SFMakeChar` / `SFSplineCharCreate` + `SFAddGlyphAndEncode`)
- [x] **SVG filter & import (`v0.3`):** `SvgSanitizer` (strips editor metadata, hidden/empty nodes, background rects, tiny specks, excludes colors or flattens to `#000000`, extracts strokes and `#ffffff` foreground cutouts) + `ImportSvgDialog` (Original vs. Filtered preview, SVG Drag & Drop, persistent last folder via `QSettings`) + `FontForgeBridge::importSvgToGlyph` (`SplinePointListInterpretSVG` / stroke expansion / overlap removal / cutout subtraction / direction correction)
- [x] **Icon image import & trace (`v0.4`):** In-memory PNG contour tracer (`BitmapTracer`: alpha/luminance thresholding, despeckling, Marching Squares boundary & hole extraction, Ramer–Douglas–Peucker simplification, corner-preserving cubic Bézier smoothing) integrated directly into `ImportSvgDialog`
- [x] **Aspect-ratio & manual icon placement (`v0.4`):** Automatic aspect-ratio positioning (`W/H <= 1.25` resting on baseline `y = 0`, `W/H > 1.25` vertically centered in ascent) + interactive `GlyphPlacementPreviewWidget` (drag to offset, wheel to scale, metric guides) + `SplinePointListTransform` in `FontForgeBridge::importSvgToGlyph`
- [ ] **Font editing / outline editor (`v0.5`):** Interactive `QGraphicsView` canvas mapping Bézier anchors/control handles directly to `SplinePoint`/`SplineSet` mutations on `FontWorker`

### Milestones

#### v0.1 (Implemented)
- [x] Import/Load Font (supported by FontForge)
- [x] Preview Glyph(s) - in Custom Glyph Area (with `.ffgui.json` sidecar persistence & Find Glyph by Unicode/Name/GID)
- [x] Export Font (supported by FontForge)

#### v0.2 (Implemented)
- [x] Edit Glyph Metadata (only name and unicodes)
- [x] Edit Font Metadata
- [x] Font Validation

#### v0.3 (Implemented)
- [x] Import SVG filter to reduce unnecessary geometries or color information (including white-foreground cutout subtraction & stroke pass handling)
- [x] Import Glyph geometry from SVG (created by Inkscape or similar program), with persistent last SVG path & SVG Drag & Drop in `ImportSvgDialog`
- [x] Drag & Drop loading of font files in empty glyph area (`GlyphGridView`)
- [x] Create New Glyph (`CreateGlyphDialog`: Unicode, Name, Alt Unicodes, Advance Width, optional post-create SVG import)

#### v0.4 (Implemented)
- [x] Save function with shortcut “CTRL+S“ to the original file
- [x] Icon positioning during import (should be centered or on the refernce line depending on width/height ratio)
- [x] Zoom in and out on Glyph area (same function as “Cell Size slider”)
- [x] Manual positioning screen/control during SVG import
- [x] PNG import function - check how complex this would be? (draw the contours of the PNG) 

#### v0.5 (Implemented)
- [x] Create New Font (initialize a new empty font in memory with customizable metadata, Ascent/Descent, and EM size)
- [x] Move glyphs function (if active, multiple glyphs can be selected and moved to a specific code point - given by unicode position or HEX number) - with warning if code points are not free
- [x] Add Supplementary Private Use Area-A (SPUA-A): U+F0000 – U+FFFFD (as a selectable range)
- [x] Add Supplementary Private Use Area-B (SPUA-B): U+100000 – U+10FFFD (as a selectable range)
- [x] Command line / shell parameters 
      - (optional) inputFontFile (if omitted but outputFontFile is given, a new font is created automatically)
      - (optional) outputFontFile (if no output file is given, it will not be saved automatically if input files are given, if just inputFontFile is given, it will open the font in GUI)
      - (optional) --headless / --gui (run batch operations headless without windows or open in GUI mode with confirmation dialogs)
      - (optional) startCodePoint - required if glyphs should get imported without JSON definition
      - (optional) gyphInputImages (directory or multiple files which will get imported)
      - (optional) glyph import parameters (used for SVGs, PNGs import filter)
      - (optional) import definition file in JSON format (where each glyph to import can be defined with code point, name, image file, filter parameters, the JSON should also allow to define a default import filter param set)
- [x] Batch import of SVGs or PNGs (where unicode start point is defined beforehand, override mode / stopps with error if code points occupied ) - should be later also be called by command line or shell

#### v0.5.1
- [ ] Improve batch import (allow to set all default import parameters - this important when the batch import is startet from GUI) 

#### v0.6
- [ ] Edit Glyph outline and vectors (simple and easy to use graphical editor)
