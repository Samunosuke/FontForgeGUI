#include "ui/GlyphInspectorPanel.hpp"

#include "model/GlyphMetadataUtils.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace ffgui {

GlyphPreviewCanvas::GlyphPreviewCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
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
    p.drawLine(QPointF(padRect.left(), originY), QPointF(padRect.right(), originY));
    p.drawLine(QPointF(originX, padRect.top()), QPointF(originX, padRect.bottom()));

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
    layout->setSpacing(8);

    m_canvas = new GlyphPreviewCanvas(this);
    layout->addWidget(m_canvas, 1);

    auto* glyphGroup = new QGroupBox(tr("Glyph Metadata & Details"), this);
    auto* glyphForm = new QFormLayout(glyphGroup);
    glyphForm->setLabelAlignment(Qt::AlignRight);
    glyphForm->setVerticalSpacing(5);

    m_nameEdit = new QLineEdit(glyphGroup);
    m_nameEdit->setPlaceholderText(tr("Glyph name (e.g. A, icon_home)"));

    auto* uniRow = new QHBoxLayout();
    uniRow->setSpacing(6);
    m_unicodeEdit = new QLineEdit(glyphGroup);
    m_unicodeEdit->setPlaceholderText(tr("U+0041 or empty for unencoded"));
    m_unicodeCharLabel = new QLabel(glyphGroup);
    m_unicodeCharLabel->setMinimumWidth(34);
    m_unicodeCharLabel->setAlignment(Qt::AlignCenter);
    uniRow->addWidget(m_unicodeEdit, 1);
    uniRow->addWidget(m_unicodeCharLabel);

    m_altUnicodesEdit = new QLineEdit(glyphGroup);
    m_altUnicodesEdit->setPlaceholderText(tr("Optional: U+0391, U+FF21"));

    m_editErrorLabel = new QLabel(glyphGroup);
    m_editErrorLabel->setStyleSheet(QStringLiteral("color: #d9534f; font-size: 11px;"));
    m_editErrorLabel->setWordWrap(true);
    m_editErrorLabel->setVisible(false);

    auto* btnRow = new QHBoxLayout();
    m_applyBtn = new QPushButton(tr("Apply"), glyphGroup);
    m_revertBtn = new QPushButton(tr("Revert"), glyphGroup);
    m_importSvgBtn = new QPushButton(tr("Import SVG…"), glyphGroup);
    m_importSvgBtn->setToolTip(tr("Filter and import vector contours from an SVG file"));
    m_importSvgBtn->setEnabled(false);
    btnRow->addWidget(m_applyBtn);
    btnRow->addWidget(m_revertBtn);
    btnRow->addWidget(m_importSvgBtn);

    m_gidLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_widthLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_geometryLabel = new QLabel(QStringLiteral("—"), glyphGroup);
    m_boundsLabel = new QLabel(QStringLiteral("—"), glyphGroup);

    glyphForm->addRow(tr("Name:"), m_nameEdit);
    glyphForm->addRow(tr("Unicode:"), uniRow);
    glyphForm->addRow(tr("Alt Unicodes:"), m_altUnicodesEdit);
    glyphForm->addRow(QString(), m_editErrorLabel);
    glyphForm->addRow(QString(), btnRow);
    glyphForm->addRow(tr("GID:"), m_gidLabel);
    glyphForm->addRow(tr("Advance:"), m_widthLabel);
    glyphForm->addRow(tr("Geometry:"), m_geometryLabel);
    glyphForm->addRow(tr("Bounds:"), m_boundsLabel);
    layout->addWidget(glyphGroup);

    auto* fontGroup = new QGroupBox(tr("Font Summary"), this);
    auto* fontLayout = new QVBoxLayout(fontGroup);
    fontLayout->setSpacing(6);
    m_fontInfoLabel = new QLabel(tr("No font loaded"), fontGroup);
    m_fontInfoLabel->setWordWrap(true);
    m_fontInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_editFontMetaBtn = new QPushButton(tr("Edit Font Metadata…"), fontGroup);
    m_editFontMetaBtn->setEnabled(false);
    fontLayout->addWidget(m_fontInfoLabel);
    fontLayout->addWidget(m_editFontMetaBtn);
    layout->addWidget(fontGroup);

    setMinimumWidth(290);

    connect(m_nameEdit, &QLineEdit::textChanged, this, &GlyphInspectorPanel::onFieldsEdited);
    connect(m_unicodeEdit, &QLineEdit::textChanged, this, &GlyphInspectorPanel::onFieldsEdited);
    connect(m_altUnicodesEdit, &QLineEdit::textChanged, this,
            &GlyphInspectorPanel::onFieldsEdited);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &GlyphInspectorPanel::onApplyClicked);
    connect(m_unicodeEdit, &QLineEdit::returnPressed, this, &GlyphInspectorPanel::onApplyClicked);
    connect(m_altUnicodesEdit, &QLineEdit::returnPressed, this,
            &GlyphInspectorPanel::onApplyClicked);
    connect(m_applyBtn, &QPushButton::clicked, this, &GlyphInspectorPanel::onApplyClicked);
    connect(m_revertBtn, &QPushButton::clicked, this, &GlyphInspectorPanel::onRevertClicked);
    connect(m_importSvgBtn, &QPushButton::clicked, this,
            &GlyphInspectorPanel::importSvgRequested);
    connect(m_editFontMetaBtn, &QPushButton::clicked, this,
            &GlyphInspectorPanel::editFontMetadataRequested);

    setGlyph(nullptr);
}

void GlyphInspectorPanel::setFontSummary(const FontSummary& summary) {
    m_fontSummary = summary;
    const QString title =
        summary.fullName.isEmpty() ? summary.fontName : summary.fullName;
    const QString splineType = summary.isOrder2 ? tr("Quadratic (TTF)") : tr("Cubic (CFF/PS)");
    m_fontInfoLabel->setText(
        QStringLiteral("<b>%1</b><br>Family: %2 · Weight: %3<br>Version: %4<br>"
                       "EM: %5 (Asc %6 / Desc %7) · %8<br>Glyphs: %9")
            .arg(title.toHtmlEscaped(), summary.familyName.toHtmlEscaped(),
                 summary.weight.toHtmlEscaped(), summary.version.toHtmlEscaped())
            .arg(summary.emSize)
            .arg(summary.ascent)
            .arg(summary.descent)
            .arg(splineType)
            .arg(summary.glyphCount));
    m_editFontMetaBtn->setEnabled(!summary.filePath.isEmpty() || !summary.fontName.isEmpty());
}

void GlyphInspectorPanel::setGlyph(const GlyphSummary* glyph) {
    if (!glyph) {
        m_hasGlyph = false;
        m_currentGlyph = GlyphSummary{};
        m_canvas->clear();
        m_updatingUi = true;
        m_nameEdit->clear();
        m_unicodeEdit->clear();
        m_unicodeCharLabel->clear();
        m_altUnicodesEdit->clear();
        m_nameEdit->setEnabled(false);
        m_unicodeEdit->setEnabled(false);
        m_altUnicodesEdit->setEnabled(false);
        m_editErrorLabel->setVisible(false);
        m_updatingUi = false;
        m_applyBtn->setEnabled(false);
        m_revertBtn->setEnabled(false);
        m_importSvgBtn->setEnabled(false);

        m_gidLabel->setText(QStringLiteral("—"));
        m_widthLabel->setText(QStringLiteral("—"));
        m_geometryLabel->setText(QStringLiteral("—"));
        m_boundsLabel->setText(QStringLiteral("—"));
        return;
    }

    m_hasGlyph = true;
    m_currentGlyph = *glyph;
    m_importSvgBtn->setEnabled(true);
    m_canvas->setGlyph(m_fontSummary, *glyph);
    populateEditorFromGlyph();

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
    m_editFontMetaBtn->setEnabled(false);
    setGlyph(nullptr);
}

void GlyphInspectorPanel::populateEditorFromGlyph() {
    m_updatingUi = true;
    m_nameEdit->setEnabled(true);
    m_unicodeEdit->setEnabled(true);
    m_altUnicodesEdit->setEnabled(true);

    m_nameEdit->setText(m_currentGlyph.name);
    m_unicodeEdit->setText(formatCodePoint(m_currentGlyph.unicode));

    QStringList alts;
    for (int32_t cp : m_currentGlyph.altUnicodes) {
        alts.append(formatCodePoint(cp));
    }
    m_altUnicodesEdit->setText(alts.join(QStringLiteral(", ")));
    m_updatingUi = false;

    onFieldsEdited();
}

bool GlyphInspectorPanel::parseEditorFields(GlyphMetadataUpdate* outUpdate, QString* outError) const {
    if (!m_hasGlyph) {
        return false;
    }

    const QString name = m_nameEdit->text().trimmed();
    if (!validateGlyphName(name, outError)) {
        return false;
    }

    int32_t primaryUni = -1;
    if (!parseSingleCodePoint(m_unicodeEdit->text(), &primaryUni)) {
        if (outError) {
            *outError = tr("Invalid Unicode (use hex like U+0041 or empty).");
        }
        return false;
    }

    QList<int32_t> alts;
    if (!parseAltUnicodesList(m_altUnicodesEdit->text(), &alts, outError)) {
        return false;
    }

    if (outUpdate) {
        outUpdate->gid = m_currentGlyph.gid;
        outUpdate->name = name;
        outUpdate->unicode = primaryUni;
        outUpdate->altUnicodes = alts;
    }
    return true;
}

void GlyphInspectorPanel::onFieldsEdited() {
    if (m_updatingUi || !m_hasGlyph) {
        return;
    }

    m_unicodeCharLabel->setText(formatUnicodePreviewBadge(m_unicodeEdit->text()));
    updateActionButtons();
}

void GlyphInspectorPanel::updateActionButtons() {
    if (!m_hasGlyph) {
        m_applyBtn->setEnabled(false);
        m_revertBtn->setEnabled(false);
        m_editErrorLabel->setVisible(false);
        return;
    }

    GlyphMetadataUpdate parsed;
    QString err;
    const bool valid = parseEditorFields(&parsed, &err);

    m_editErrorLabel->setVisible(!valid);
    m_editErrorLabel->setText(err);

    const bool dirty = !valid || parsed.name != m_currentGlyph.name ||
                       parsed.unicode != m_currentGlyph.unicode ||
                       parsed.altUnicodes != m_currentGlyph.altUnicodes;

    m_applyBtn->setEnabled(valid && dirty);
    m_revertBtn->setEnabled(dirty);
}

void GlyphInspectorPanel::onApplyClicked() {
    if (!m_hasGlyph) {
        return;
    }
    GlyphMetadataUpdate update;
    QString err;
    if (!parseEditorFields(&update, &err)) {
        return;
    }
    if (update.name == m_currentGlyph.name && update.unicode == m_currentGlyph.unicode &&
        update.altUnicodes == m_currentGlyph.altUnicodes) {
        return;
    }
    emit glyphMetadataEditRequested(update);
}

void GlyphInspectorPanel::onRevertClicked() {
    if (m_hasGlyph) {
        populateEditorFromGlyph();
    }
}

} // namespace ffgui
