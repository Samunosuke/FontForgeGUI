#include "model/GlyphListModel.hpp"

#include <QVariant>

namespace ffgui {

GlyphListModel::GlyphListModel(QObject* parent) : QAbstractListModel(parent) {}

int GlyphListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_filteredIndices.size();
}

QVariant GlyphListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size()) {
        return {};
    }
    const GlyphSummary& g = m_glyphs[m_filteredIndices[index.row()]];

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return g.name;
    case GidRole:
        return g.gid;
    case UnicodeRole:
        return static_cast<int>(g.unicode);
    case WidthRole:
        return g.width;
    case OutlineRole:
        return QVariant::fromValue(g.outline);
    case HasOutlineRole:
        return g.hasOutline;
    case AscentRole:
        return m_summary.ascent;
    case DescentRole:
        return m_summary.descent;
    case EmSizeRole:
        return m_summary.emSize;
    default:
        return {};
    }
}

void GlyphListModel::setDocument(const FontDocumentSnapshot& snapshot) {
    beginResetModel();
    m_summary = snapshot.summary;
    m_glyphs = snapshot.glyphs;
    m_filteredIndices.clear();
    for (int i = 0; i < m_glyphs.size(); ++i) {
        if (m_rangeFilter.matches(m_glyphs[i]) && matchesSearch(m_glyphs[i])) {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
}

void GlyphListModel::setFontSummary(const FontSummary& summary) {
    m_summary = summary;
    if (!m_filteredIndices.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_filteredIndices.size() - 1, 0),
                         {AscentRole, DescentRole, EmSizeRole});
    }
}

bool GlyphListModel::updateGlyph(const GlyphSummary& updated) {
    int glyphIdx = -1;
    for (int i = 0; i < m_glyphs.size(); ++i) {
        if (m_glyphs[i].gid == updated.gid) {
            glyphIdx = i;
            break;
        }
    }
    if (glyphIdx < 0) {
        return false;
    }

    m_glyphs[glyphIdx] = updated;
    const bool shouldBeVisible =
        m_rangeFilter.matches(m_glyphs[glyphIdx]) && matchesSearch(m_glyphs[glyphIdx]);
    const int existingRow = rowForGid(updated.gid);

    if (shouldBeVisible && existingRow >= 0) {
        const QModelIndex idx = index(existingRow, 0);
        emit dataChanged(idx, idx);
    } else if (shouldBeVisible != (existingRow >= 0)) {
        rebuildFilter();
    }
    return true;
}

void GlyphListModel::addGlyph(const GlyphSummary& added, const FontSummary& updatedSummary) {
    m_summary = updatedSummary;
    int existingIdx = -1;
    for (int i = 0; i < m_glyphs.size(); ++i) {
        if (m_glyphs[i].gid == added.gid) {
            existingIdx = i;
            break;
        }
    }
    if (existingIdx >= 0) {
        m_glyphs[existingIdx] = added;
    } else {
        m_glyphs.append(added);
    }
    rebuildFilter();
}

void GlyphListModel::clear() {
    beginResetModel();
    m_summary = FontSummary{};
    m_glyphs.clear();
    m_filteredIndices.clear();
    endResetModel();
}

void GlyphListModel::setRangeFilter(const UnicodeRangeFilter& filter) {
    m_rangeFilter = filter;
    rebuildFilter();
}

void GlyphListModel::setSearchText(const QString& query) {
    const QString trimmed = query.trimmed();
    if (m_searchQuery == trimmed) {
        return;
    }
    m_searchQuery = trimmed;
    rebuildFilter();
}

const GlyphSummary* GlyphListModel::glyphAtRow(int row) const {
    if (row < 0 || row >= m_filteredIndices.size()) {
        return nullptr;
    }
    return &m_glyphs[m_filteredIndices[row]];
}

int GlyphListModel::rowForGid(int gid) const {
    for (int row = 0; row < m_filteredIndices.size(); ++row) {
        if (m_glyphs[m_filteredIndices[row]].gid == gid) {
            return row;
        }
    }
    return -1;
}

void GlyphListModel::rebuildFilter() {
    beginResetModel();
    m_filteredIndices.clear();
    m_filteredIndices.reserve(m_glyphs.size());
    for (int i = 0; i < m_glyphs.size(); ++i) {
        if (m_rangeFilter.matches(m_glyphs[i]) && matchesSearch(m_glyphs[i])) {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
}

bool GlyphListModel::matchesSearch(const GlyphSummary& glyph) const {
    if (m_searchQuery.isEmpty()) {
        return true;
    }
    if (glyph.name.contains(m_searchQuery, Qt::CaseInsensitive)) {
        return true;
    }

    // Match GID (e.g. "42" or "#42")
    QString gidStr = m_searchQuery;
    if (gidStr.startsWith(QChar('#'))) {
        gidStr = gidStr.mid(1);
    }
    bool numOk = false;
    const int parsedGid = gidStr.toInt(&numOk, 10);
    if (numOk && parsedGid == glyph.gid) {
        return true;
    }

    // Match Unicode code point (e.g. "U+0041", "0041", "0x41")
    QString hexStr = m_searchQuery;
    if (hexStr.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) ||
        hexStr.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        hexStr = hexStr.mid(2);
    }
    bool hexOk = false;
    const uint32_t cp = hexStr.toUInt(&hexOk, 16);
    if (hexOk && cp <= 0x10FFFFu) {
        if (glyph.unicode == static_cast<int32_t>(cp)) {
            return true;
        }
        if (glyph.altUnicodes.contains(static_cast<int32_t>(cp))) {
            return true;
        }
    }

    // Match single typed character (e.g. "A")
    if (m_searchQuery.size() == 1) {
        const uint32_t ch = m_searchQuery.at(0).unicode();
        if (glyph.unicode == static_cast<int32_t>(ch) ||
            glyph.altUnicodes.contains(static_cast<int32_t>(ch))) {
            return true;
        }
    }

    return false;
}

} // namespace ffgui

