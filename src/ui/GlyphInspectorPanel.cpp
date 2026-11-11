#include "ui/GlyphInspectorPanel.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace ffgui {

GlyphPreviewCanvas::GlyphPreviewCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(230);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void GlyphPreviewCanvas::setGlyph(const FontSummary& font, const GlyphSummary& glyph) {
    m_hasData = true;
    m_font = font;
    m_glyph = glyph;
    update();
}

void GlyphPreviewCanvas::clear() {
    m_hasData = false;
    m_glyph = GlyphSummary{};
    update();
}

void GlyphPreviewCanvas::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPalette& pal = palette();
    const QRectF r = rect().adjusted(1, 1, -1, -1);

    p.setPen(QPen(pal.mid().color(), 1.0));
    p.setBrush(pal.base().color());
    p.drawRoundedRect(r, 6.0, 6.0);

    if (!m_hasData) {
        QColor muted = pal.text().color();
        muted.setAlpha(120);
        p.setPen(muted);
        p.drawText(r, Qt::AlignCenter, tr("Select a glyph to inspect"));
        return;
    }

    const QRectF padRect = r.adjusted(18, 18, -18, -18);
    const double emSize = std::max(1, m_font.emSize);

    // Compute scale so both EM box and actual glyph bounds fit comfortably
    double minX = 0.0;
    double maxX = std::max<double>(m_glyph.width, emSize * 0.6);
    double minY = -static_cast<double>(m_font.descent);
    double maxY = static_cast<double>(m_font.ascent);

    if (m_glyph.hasOutline) {
        minX = std::min(minX, m_glyph.bounds.left());
        maxX = std::max(maxX, m_glyph.bounds.right());
        minY = std::min(minY, m_glyph.bounds.top());
        maxY = std::max(maxY, m_glyph.bounds.bottom());
    }

    const double spanX = std::max(1.0, maxX - minX);
    const double spanY = std::max(1.0, maxY - minY);
    const double scale = std::min(padRect.width() / spanX, padRect.height() / spanY) * 0.9;

    const double originX = padRect.center().x() - ((minX + maxX) * 0.5) * scale;
    const double originY = padRect.center().y() + ((minY + maxY) * 0.5) * scale;

    auto toScreen = [&](double fx, double fy) {
        return QPointF(originX + fx * scale, originY - fy * scale);
    };

    // Metric guides: Ascent, Baseline, Descent, Origin X=0, Advance Width
    QColor metricColor = pal.mid().color();
    metricColor.setAlpha(120);
    p.setPen(QPen(metricColor, 1.0, Qt::DashLine));
    p.drawLine(QPointF(padRect.left(), toScreen(0, m_font.ascent).y()),
               QPointF(padRect.right(), toScreen(0, m_font.ascent).y()));
    p.drawLine(QPointF(padRect.left(), toScreen(0, -m_font.descent).y()),
               QPointF(padRect.right(), toScreen(0, -m_font.descent).y()));

    QColor axisColor = pal.highlight().color();
    axisColor.setAlpha(140);
    p.setPen(QPen(axisColor, 1.0));
    p.drawLine(QPointF(padRect.left(), originY), QPointF(padRect.right(), originY)); // baseline
    p.drawLine(QPointF(originX, padRect.top()), QPointF(originX, padRect.bottom())); // x=0

    // Advance width line
    const double advX = toScreen(m_glyph.width, 0).x();
    QColor advColor(46, 160, 67, 170);
    p.setPen(QPen(advColor, 1.2, Qt::DashDotLine));
    p.drawLine(QPointF(advX, padRect.top()), QPointF(advX, padRect.bottom()));

    if (m_glyph.hasOutline) {
        p.save();
        p.translate(originX, originY);
        p.scale(scale, -scale);
        QColor fillColor = pal.text().color();
        fillColor.setAlpha(215);
        p.setPen(Qt::NoPen);
        p.setBrush(fillColor);
        p.drawPath(m_glyph.outline);
        p.restore();
    }
}

GlyphInspectorPanel::GlyphInspectorPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    m_canvas = new GlyphPreviewCanvas(this);
    layout->addWidget(m_canvas, 1);

    auto* glyphGroup = new QGroupBox(tr("Glyph Details"), this);
    auto* glyphForm = new QFormLayout(glyphGroup);
    glyphForm->setLabelAlignment(Qt::AlignRight);
    m_glyphNameLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_glyphNameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_unicodeLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_unicodeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_gidLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_widthLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_geometryLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_boundsLabel = new QLabel(QStringLiteral("—"), glyphGroup);

    glyphForm->addRow(tr("Name:"), m_glyphNameLabel);
    glyphForm->addRow(tr("Unicode:"), m_unicodeLabel);
    glyphForm->addRow(tr("GID:"), m_gidLabel);
    glyphForm->addRow(tr("Advance:"), m_widthLabel);
    glyphForm->addRow(tr("Geometry:"), m_geometryLabel);
    glyphForm->addRow(tr("Bounds:"), m_boundsLabel);
    layout->addWidget(glyphGroup);

    auto* fontGroup = new QGroupBox(tr("Font Summary"), this);
    auto* fontForm = new QFormLayout(fontGroup);
    m_fontInfoLabel = new QLabel(tr("No font loaded"), fontGroup);
    m_fontInfoLabel->setWordWrap(true);
    m_fontInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fontForm->addRow(m_fontInfoLabel);
    layout->addWidget(fontGroup);

    setMinimumWidth(280);
}

void GlyphInspectorPanel::setFontSummary(const FontSummary& summary) {
    m_fontSummary = summary;
    const QString title =
        summary.fullName.isEmpty() ? summary.fontName : summary.fullName;
    const QString splineType = summary.isOrder2 ? tr("Quadratic (TTF)") : tr("Cubic (CFF/PS)");
    m_fontInfoLabel->setText(
        QStringLiteral("<b>%1</b><br>Family: %2<br>Weight: %3<br>Version: %4<br>"
                       "EM: %5 (Asc %6 / Desc %7)<br>Splines: %8<br>Glyphs: %9")
            .arg(title.toHtmlEscaped(), summary.familyName.toHtmlEscaped(),
                 summary.weight.toHtmlEscaped(), summary.version.toHtmlEscaped())
            .arg(summary.emSize)
            .arg(summary.ascent)
            .arg(summary.descent)
            .arg(splineType)
            .arg(summary.glyphCount));
}

void GlyphInspectorPanel::setGlyph(const GlyphSummary* glyph) {
    if (!glyph) {
        m_canvas->clear();
        m_glyphNameLabel->setText(QStringLiteral("—"));
        m_unicodeLabel->setText(QStringLiteral("—"));
        m_gidLabel->setText(QStringLiteral("—"));
        m_widthLabel->setText(QStringLiteral("—"));
        m_geometryLabel->setText(QStringLiteral("—"));
        m_boundsLabel->setText(QStringLiteral("—"));
        return;
    }

    m_canvas->setGlyph(m_fontSummary, *glyph);
    m_glyphNameLabel->setText(glyph->name.isEmpty() ? QStringLiteral("(unnamed)") : glyph->name);

    if (glyph->unicode >= 0) {
        QString uniStr =
            QStringLiteral("U+%1").arg(glyph->unicode, 4, 16, QLatin1Char('0')).toUpper();
        if (glyph->unicode >= 0x20 && glyph->unicode <= 0x10FFFF) {
            uniStr += QStringLiteral(" ('%1')").arg(QString::fromUcs4(
                reinterpret_cast<const char32_t*>(&glyph->unicode), 1));
        }
        if (!glyph->altUnicodes.isEmpty()) {
            uniStr += QStringLiteral(" +%1 alt").arg(glyph->altUnicodes.size());
        }
        m_unicodeLabel->setText(uniStr);
    } else {
        m_unicodeLabel->setText(tr("Unencoded"));
    }

    m_gidLabel->setText(QString::number(glyph->gid));
    m_widthLabel->setText(QString::number(glyph->width));
    m_geometryLabel->setText(tr("%1 contours, %2 pts, %3 refs")
                                 .arg(glyph->contourCount)
                                 .arg(glyph->pointCount)
                                 .arg(glyph->referenceCount));
    if (glyph->hasOutline) {
        m_boundsLabel->setText(QStringLiteral("[%1, %2] .. [%3, %4]")
                                   .arg(std::lround(glyph->bounds.left()))
                                   .arg(std::lround(glyph->bounds.top()))
                                   .arg(std::lround(glyph->bounds.right()))
                                   .arg(std::lround(glyph->bounds.bottom())));
    } else {
        m_boundsLabel->setText(tr("(empty)"));
    }
}

void GlyphInspectorPanel::clear() {
    m_fontSummary = FontSummary{};
    m_fontInfoLabel->setText(tr("No font loaded"));
    setGlyph(nullptr);
}

} // namespace ffgui

