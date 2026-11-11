# FontForgeGUI

Modern cross-platform Qt 6 GUI for FontForge, built directly on headless `libfontforge` (`ENABLE_GUI=OFF`).

## Architecture (v0.1)

- `backend/` — Thread-safe `FontWorker` (dedicated `QThread`) wrapping `FontForgeBridge` (`libfontforge` C API + `ui_interface` hooks).
- `model/` — `UnicodeRangeFilter`, `CustomGlyphAreaConfig` (`.ffgui.json` sidecar persistence), and `GlyphListModel` (`QAbstractListModel`).
- `ui/` — `MainWindow`, `GlyphGridDelegate` (vector `QPainterPath` preview cards), `GlyphInspectorPanel`, and `ExportFontDialog`.
- `tests/` — CTest suite (`backend_smoke`, `unicode_range`).

## Build & Run (Ubuntu / Linux)

FontForge is referenced from the sibling repository `../FontForge` by default (override with `-DFONTFORGE_SOURCE_DIR=/path/to/FontForge`).

```bash
cmake -G Ninja -S src -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/FontForgeGUI
```

You can also pass a font file on the command line:

```bash
./build/FontForgeGUI ../FontForge/tests/fonts/Ambrosia.sfd
```

