#pragma once

#include "backend/FontTypes.hpp"
#include "model/UnicodeRangeFilter.hpp"

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace ffgui {

class GlyphListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        GidRole = Qt::UserRole + 1,
        UnicodeRole,
        NameRole,
        WidthRole,
        OutlineRole,
        HasOutlineRole,
        AscentRole,
        DescentRole,
        EmSizeRole
    };

    explicit GlyphListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setDocument(const FontDocumentSnapshot& snapshot);
    void setFontSummary(const FontSummary& summary);
    bool updateGlyph(const GlyphSummary& updated);
    void clear();

    void setRangeFilter(const UnicodeRangeFilter& filter);
    void setSearchText(const QString& query);

    const FontSummary& fontSummary() const { return m_summary; }
    int totalGlyphCount() const { return m_glyphs.size(); }
    int filteredGlyphCount() const { return m_filteredIndices.size(); }

    const GlyphSummary* glyphAtRow(int row) const;
    int rowForGid(int gid) const;

private:
    void rebuildFilter();
    bool matchesSearch(const GlyphSummary& glyph) const;

    FontSummary m_summary;
    QVector<GlyphSummary> m_glyphs;
    QVector<int> m_filteredIndices;
    UnicodeRangeFilter m_rangeFilter;
    QString m_searchQuery;
};

} // namespace ffgui

