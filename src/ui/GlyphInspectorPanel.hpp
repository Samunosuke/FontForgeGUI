#pragma once

#include "backend/FontTypes.hpp"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace ffgui {

class GlyphPreviewCanvas : public QWidget {
    Q_OBJECT

public:
    explicit GlyphPreviewCanvas(QWidget* parent = nullptr);

    void setGlyph(const FontSummary& font, const GlyphSummary& glyph);
    void clear();

    QSize minimumSizeHint() const override { return QSize(220, 200); }

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

signals:
    void glyphMetadataEditRequested(const ffgui::GlyphMetadataUpdate& update);
    void editFontMetadataRequested();

private slots:
    void onFieldsEdited();
    void onApplyClicked();
    void onRevertClicked();

private:
    void populateEditorFromGlyph();
    bool parseEditorFields(GlyphMetadataUpdate* outUpdate, QString* outError) const;
    void updateActionButtons();

    GlyphPreviewCanvas* m_canvas = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_unicodeEdit = nullptr;
    QLabel* m_unicodeCharLabel = nullptr;
    QLineEdit* m_altUnicodesEdit = nullptr;
    QLabel* m_editErrorLabel = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_revertBtn = nullptr;

    QLabel* m_gidLabel = nullptr;
    QLabel* m_widthLabel = nullptr;
    QLabel* m_geometryLabel = nullptr;
    QLabel* m_boundsLabel = nullptr;
    QLabel* m_fontInfoLabel = nullptr;
    QPushButton* m_editFontMetaBtn = nullptr;

    FontSummary m_fontSummary;
    bool m_hasGlyph = false;
    GlyphSummary m_currentGlyph;
    bool m_updatingUi = false;
};

} // namespace ffgui
