# FontForgeGUI Command-Line Interface (CLI) Reference

`FontForgeGUI` provides a command-line interface that allows developers, font creators, and automated CI/CD pipelines to create, open, merge, transform, and export fonts headlessly or pre-configure the interactive Qt GUI.

---

## Synopsis

```sh
# Headless font creation and image batch import
FontForgeGUI [options] -o <output-font> [files / directories]

# Headless font merging
FontForgeGUI -i <base-font> --merge-font <source-font> -o <output-font> [options]

# Interactive GUI startup with pre-loaded font
FontForgeGUI [font-file]
FontForgeGUI -i <font-file>
```

---

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | Success (Font created/imported/merged/exported successfully) |
| `1` | Error (File not found, invalid syntax/Unicode code point, parsing or export failure) |

---

## Global Options

### Help & Version
* `-h`, `--help`
  Displays the command-line help banner describing all options and exits.
* `-v`, `--version`
  Displays application version information (e.g. `FontForgeGUI 0.5.3`) and exits.

### Execution Modes
* `--headless`
  Forces headless mode without creating or displaying any GUI windows. By default, headless mode is automatically selected whenever `--output` is specified alongside batch operations (`--import-images`, `--import-def`, or `--merge-font`).
* `--gui`
  Forces opening the interactive Qt 6 graphical interface even if batch or merge arguments are provided. When specified, any batch jobs will populate their respective GUI dialogs for visual inspection before applying.

---

## Font Input and Output Options

* `-i <file>`, `--input <file>`
  Specifies the base font file to open (`.ttf`, `.otf`, `.sfd`, `.woff`, `.woff2`, `.pfb`, etc.).
  * If omitted and `--output` is specified, FontForgeGUI creates a brand new blank font document using the output filename as the font and family name.
  * Can also be passed as a standalone positional argument (e.g. `FontForgeGUI myfont.ttf`).
* `-o <file>`, `--output <file>`
  Specifies the destination path where the modified or generated font should be saved.
  * Supported output formats: `.ttf` (TrueType), `.otf` (OpenType/CFF), `.woff` / `.woff2` (Web Fonts), `.sfd` (FontForge Spline Font Database), `.ufo` (Unified Font Object), `.svg` (SVG Font).

---

## Font Merging Options

FontForgeGUI allows merging glyphs from a secondary font into the active font with exact vector outline preservation, automatic EM-unit scaling, and metrics copying.

* `--merge-font <file>`
  Specifies the path to the source font file to merge glyphs from.
* `--source-range <range>`
  Specifies which glyphs to extract from the source font. Defaults to `all`.
  * Accepts `all` or `*` to import all encoded and unencoded glyphs.
  * Accepts comma-separated Unicode ranges or hex/decimal values:
    * `U+E000-U+E0FF`
    * `0xF000-0xF050, 0xF100-0xF120`
    * `65-90` (ASCII A-Z)
* `--target-start <unicode>`
  Specifies the target starting Unicode code point where merged glyphs should be remapped sequentially (e.g. `U+F0000` or `0xE000`).
  * If omitted, merged glyphs retain their original source Unicode code points.
* `--no-scale-to-em`
  Disables proportional scaling of vector outlines from the source font's EM unit size to the target font's EM unit size. By default, glyph outlines are scaled automatically (e.g. 1000 EM to 2048 EM).
* `--no-copy-metrics`
  Disables copying horizontal advance width metrics (`width`) from the source font. By default, advance widths are scaled and copied.
* `--overwrite`
  Allows overwriting existing glyphs in the target font if a destination code point collision occurs. If omitted and a collision is detected, the operation aborts with an error message to prevent accidental data loss.

---

## Batch Image Import Options (SVG / PNG)

Import vector SVGs or raster images (PNG, JPG, BMP) into font glyph slots.

* `-m <path>`, `--import-images <path>`
  Path to an image directory or a comma-separated list of image files.
  * When a directory is given, all `.svg` and `.png` files within it are imported in alphabetical order.
  * Example: `--import-images ./assets/icons` or `--import-images icon1.svg,icon2.svg,icon3.png`
* `-d <file.json>`, `--import-def <file.json>`
  Specifies a JSON file describing a batch import job with per-glyph properties (names, target unicodes, advance widths, placement, and vectorization settings).
* `-u <unicode>`, `--start-unicode <unicode>`
  Starting Unicode code point for imported images when importing a list or directory. Defaults to `U+F0000` (Private Use Area).
  * Accepts `U+XXXX` or `0xXXXX` hex format (e.g. `U+E000`).
* `--placement <mode>`
  Specifies the glyph placement algorithm for imported SVGs/images.
  * `auto`: Automatic aspect ratio fit preserving proportional bounds (default).
  * `baseline`: Aligns glyph baseline to font baseline (Y = 0) and centers horizontally.
  * `center-ascent`: Centers glyph within the font ascent box.
  * `center-em`: Centers glyph within the entire EM square (`ascent + descent`).
  * `raw`: Preserves raw SVG coordinates directly without bounding box transformation.
* `--scale-factor <factor>`
  Scale multiplier applied to imported glyph contours (e.g. `1.0` = 100%, `0.85` = 85%). Defaults to `1.0`.
* `--overwrite`
  Allows replacing existing glyphs if the target code point is already occupied.

---

## Usage Examples

### 1. Merge Private Use Area Icons from an Icon Font into a Text Font
Merge icons from `material-icons.ttf` (range `U+E000-U+E0FF`) into `Roboto-Regular.ttf`, placing them into Unicode starting at `U+F000` with EM scaling:
```sh
FontForgeGUI -i Roboto-Regular.ttf \
             --merge-font material-icons.ttf \
             --source-range U+E000-U+E0FF \
             --target-start U+F000 \
             -o Roboto-Merged.ttf
```

### 2. Create a New Web Font from an SVG Icon Folder
Generate a clean new `woff2` font containing all icons from a directory, assigned sequentially starting at `U+E000`:
```sh
FontForgeGUI -o dist/myicons.woff2 \
             --import-images ./src/icons \
             --start-unicode U+E000 \
             --placement baseline \
             --scale-factor 0.90
```

### 3. Batch Import via JSON Definition
Use a structured JSON manifest to define custom names, tags, and codepoints:
```sh
FontForgeGUI -i BaseFont.ttf \
             --import-def icons-manifest.json \
             -o OutputFont.woff2 \
             --overwrite
```

### 4. Interactive GUI Pre-Configuration
Launch FontForgeGUI with a font loaded and pre-open the batch import dialog with a directory of SVG files:
```sh
FontForgeGUI -i CustomFont.sfd --import-images ./new-glyphs --gui
```

