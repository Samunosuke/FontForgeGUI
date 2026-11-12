#pragma once

#include <QList>
#include <QString>

#include <cstdint>

namespace ffgui {

QString formatCodePoint(int32_t cp);
bool parseSingleCodePoint(QString text, int32_t* outCp);
bool parseAltUnicodesList(const QString& text, QList<int32_t>* outAlts, QString* outError);
bool validateGlyphName(const QString& name, QString* outError, bool allowEmpty = false);
QString suggestedGlyphName(int32_t unicode);
QString formatUnicodePreviewBadge(const QString& unicodeText);

} // namespace ffgui

