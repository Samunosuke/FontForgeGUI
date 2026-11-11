# FontForgeGUI concept

FontForgeGUI is a GUI application for FontForge. It provides a user-friendly interface for editing fonts.

It uses the fontForge code as a backend and provides a GUI for it.

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
    - Basically implemented as a range/s of Unicode code points (it filters als glyphs in the font)
    - It can be changed dynamically
    - It will be saved in the font file or in separate file if font file does not support it

## UI

The UI is designed to be simple and easy to use.

- Cross Plattform (Windows / Linux)
- Qt5 ? Web based?

### Questions
- What UI framework should I use? 
- How to implement the font editing features? 
- How to implement the font exporting features? 
- How to implement the font importing features? 
- How to implement the font validation features? 
- How to implement the font preview features? 
- How to implement the import svg file and convert to font glyph features? 
- How to implement the import icon image and convert to font glyph features? 
- How to implement the glyph metadata editor with glyph preview features? 
- How to implement the font metadata editor features? 
- How to implement the find glyph by unicode, name, position features? 

### Milestones

#### v0.1
- Import/Load Font (supported by FontForge)
- Preview Glyph(s) - in Custom Glyph Area
- Export Font (supported by FontForge)

#### v0.2
- Edit Glyph Metadata (only name and unicodes)
- Edit Font Metadata
- Font Validation

#### v0.3
- Import SVG filter to reduce unnecessary geometries or colors information
- Import Glyph geometry from SVG (created by Inkscape or similar program)

#### v0.4
- Edit Glyph outline (simple and easy to use editor)
