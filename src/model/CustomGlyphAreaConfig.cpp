#include "model/CustomGlyphAreaConfig.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace ffgui {

QString CustomGlyphAreaConfig::sidecarPathForFont(const QString& fontPath) {
    if (fontPath.isEmpty()) {
        return {};
    }
    return fontPath + QStringLiteral(".ffgui.json");
}

CustomGlyphAreaConfig CustomGlyphAreaConfig::createDefault() {
    CustomGlyphAreaConfig cfg;
    cfg.activeIndex = 0;
    cfg.areas = {
        {QStringLiteral("All Glyphs"), QStringLiteral("*"), true},
        {QStringLiteral("Basic Latin"), QStringLiteral("U+0020-007E"), false},
        {QStringLiteral("Icons / PUA"), QStringLiteral("U+E000-F8FF"), false},
        {QStringLiteral("SPUA-A"), QStringLiteral("U+F0000-FFFFD"), false},
        {QStringLiteral("SPUA-B"), QStringLiteral("U+100000-10FFFD"), false},
    };
    return cfg;
}

bool CustomGlyphAreaConfig::loadForFont(const QString& fontPath, CustomGlyphAreaConfig* outConfig,
                                        QString* outError) {
    if (!outConfig) {
        return false;
    }
    const QString sidecarPath = sidecarPathForFont(fontPath);
    if (sidecarPath.isEmpty() || !QFile::exists(sidecarPath)) {
        *outConfig = createDefault();
        return false;
    }

    QFile file(sidecarPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) {
            *outError = QStringLiteral("Cannot open %1: %2").arg(sidecarPath, file.errorString());
        }
        *outConfig = createDefault();
        return false;
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) {
            *outError =
                QStringLiteral("Invalid JSON in %1: %2").arg(sidecarPath, parseErr.errorString());
        }
        *outConfig = createDefault();
        return false;
    }

    const QJsonObject root = doc.object();
    CustomGlyphAreaConfig loaded;
    const QJsonArray arr = root.value(QStringLiteral("areas")).toArray();
    for (const QJsonValue& val : arr) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        CustomGlyphArea area;
        area.name = obj.value(QStringLiteral("name")).toString();
        area.rangeSpec = obj.value(QStringLiteral("rangeSpec")).toString(QStringLiteral("*"));
        area.includeUnencoded = obj.value(QStringLiteral("includeUnencoded")).toBool(false);
        if (!area.name.isEmpty()) {
            loaded.areas.append(area);
        }
    }

    if (loaded.areas.isEmpty()) {
        *outConfig = createDefault();
        return false;
    }

    const int idx = root.value(QStringLiteral("activeIndex")).toInt(0);
    loaded.activeIndex = (idx >= 0 && idx < loaded.areas.size()) ? idx : 0;
    *outConfig = std::move(loaded);
    return true;
}

bool CustomGlyphAreaConfig::saveForFont(const QString& fontPath, QString* outError) const {
    const QString sidecarPath = sidecarPathForFont(fontPath);
    if (sidecarPath.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("Font path is empty.");
        }
        return false;
    }

    QJsonArray arr;
    for (const CustomGlyphArea& area : areas) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), area.name);
        obj.insert(QStringLiteral("rangeSpec"), area.rangeSpec);
        obj.insert(QStringLiteral("includeUnencoded"), area.includeUnencoded);
        arr.append(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("activeIndex"), activeIndex);
    root.insert(QStringLiteral("areas"), arr);

    QSaveFile file(sidecarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (outError) {
            *outError = QStringLiteral("Cannot write %1: %2").arg(sidecarPath, file.errorString());
        }
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (outError) {
            *outError = QStringLiteral("Failed to commit %1: %2").arg(sidecarPath, file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace ffgui

