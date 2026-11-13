#pragma once

#include <QApplication>
#include <QIcon>
#include <QPalette>
#include <QString>

namespace ffgui {

/**
 * Loads an embedded monochrome SVG icon from Qt resources (e.g. ":/icons/save.svg")
 * and tints `currentColor` to match the given QPalette's normal and disabled
 * foreground colors across crisp multi-DPI sizes.
 */
QIcon loadTintedSvgIcon(const QString& resourcePath,
                        const QPalette& palette = QApplication::palette());

/**
 * Renders a tinted SVG icon as a QPixmap at the requested logical size.
 */
QPixmap loadTintedSvgPixmap(const QString& resourcePath, int logicalSize = 16,
                            const QColor& color = QApplication::palette().color(QPalette::WindowText));

} // namespace ffgui

