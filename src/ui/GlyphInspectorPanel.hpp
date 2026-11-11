#pragma once

#include "backend/FontTypes.hpp"

#include <QWidget>

class QLabel;

namespace ffgui {

class GlyphPreviewCanvas : public QWidget {
    Q_OBJECT

public:
    explicit GlyphPreviewCanvas(QWidget* parent = nullptr);

    void setGlyph(const FontSummary& font, const GlyphSummary& glyph);
    void clear();

    QSize minimumSizeHint() const override { return QSize(220, 220); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_hasData = false;
    FontSummary m_font;
    GlyphSummary m_glyph;
};

class GlyphInspectorPanel : public QWidget {
    Q_OBJECT

public:
    explicit GlyphInspectorPanel(QWidget* parent = nullptr);

    void setFontSummary(const FontSummary& summary);
    void setGlyph(const GlyphSummary* glyph);
    void clear();

private:
    GlyphPreviewCanvas* m_canvas = nullptr;
    QLabel* m_glyphNameLabel = nullptr;
    QLabel* m_unicodeLabel = nullptr;
    QLabel* m_gidLabel = nullptr;
    QLabel* m_widthLabel = nullptr;
    QLabel* m_geometryLabel = nullptr;
    QLabel* m_boundsLabel = nullptr;
    QLabel* m_fontInfoLabel = nullptr;

    FontSummary m_fontSummary;
};

} // namespace ffgui

