#include "ui/IconUtils.hpp"

#include <QByteArray>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace ffgui {

namespace {

QByteArray loadTintedSvgBytes(const QString& resourcePath, const QColor& color) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray svgBytes = file.readAll();
    const QByteArray hexColor = color.name(QColor::HexRgb).toLatin1();
    svgBytes.replace("currentColor", hexColor);
    return svgBytes;
}

QPixmap renderSvgBytes(const QByteArray& svgBytes, int size) {
    if (svgBytes.isEmpty()) {
        return {};
    }
    QSvgRenderer renderer(svgBytes);
    if (!renderer.isValid()) {
        return {};
    }
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter);
    return pm;
}

} // namespace

QIcon loadTintedSvgIcon(const QString& resourcePath, const QPalette& palette) {
    const QColor normalColor = palette.color(QPalette::Active, QPalette::ButtonText);
    const QColor disabledColor = palette.color(QPalette::Disabled, QPalette::ButtonText);

    const QByteArray normalBytes = loadTintedSvgBytes(resourcePath, normalColor);
    const QByteArray disabledBytes = loadTintedSvgBytes(resourcePath, disabledColor);

    QIcon icon;
    for (int sz : {16, 20, 24, 32}) {
        if (!normalBytes.isEmpty()) {
            const QPixmap normalPm = renderSvgBytes(normalBytes, sz);
            icon.addPixmap(normalPm, QIcon::Normal, QIcon::Off);
            icon.addPixmap(normalPm, QIcon::Active, QIcon::Off);
        }
        if (!disabledBytes.isEmpty()) {
            icon.addPixmap(renderSvgBytes(disabledBytes, sz), QIcon::Disabled, QIcon::Off);
        }
    }
    return icon;
}

QPixmap loadTintedSvgPixmap(const QString& resourcePath, int logicalSize, const QColor& color) {
    return renderSvgBytes(loadTintedSvgBytes(resourcePath, color), logicalSize);
}

} // namespace ffgui

