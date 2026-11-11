#pragma once

#include <QList>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <QVector>
#include <cstdint>

namespace ffgui {

struct FontSummary {
    QString filePath;
    QString fontName;
    QString familyName;
    QString fullName;
    QString weight;
    QString version;
    QString copyright;
    int ascent = 800;
    int descent = 200;
    int emSize = 1000;
    int glyphCount = 0;
    bool isOrder2 = false;
};

struct GlyphSummary {
    int gid = -1;
    int32_t unicode = -1; // -1 if unencoded
    QList<int32_t> altUnicodes;
    QString name;
    int width = 0;
    int vwidth = 0;
    int contourCount = 0;
    int pointCount = 0;
    int referenceCount = 0;
    QPainterPath outline; // In font coordinates (Y-up), fillRule = Qt::WindingFill
    QRectF bounds;
    bool hasOutline = false;
};

struct FontDocumentSnapshot {
    FontSummary summary;
    QVector<GlyphSummary> glyphs;
};

struct ExportOptions {
    QString outputPath;
    bool roundToInt = true;
    bool opentypeTables = true;
    bool omitHints = false;
    bool dummyDsig = true;
    bool generateAfm = false;
};

} // namespace ffgui

