#include "backend/FontForgeBridge.hpp"
#include "model/BatchImportDefinition.hpp"
#include "ui/MainWindow.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <iostream>

namespace {

int32_t parseCliUnicode(QString s) {
    s = s.trimmed();
    if (s.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) ||
        s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        s = s.mid(2);
    }
    bool ok = false;
    uint32_t val = s.toUInt(&ok, 16);
    if (!ok || val > 0x10FFFFu) {
        return -1;
    }
    return static_cast<int32_t>(val);
}

ffgui::GlyphPlacementMode parseCliPlacement(const QString& str) {
    const QString s = str.toLower();
    if (s == QStringLiteral("baseline") || s == QStringLiteral("baselinecentered")) {
        return ffgui::GlyphPlacementMode::BaselineCentered;
    }
    if (s == QStringLiteral("center-ascent") || s == QStringLiteral("ascent")) {
        return ffgui::GlyphPlacementMode::CenterInAscent;
    }
    if (s == QStringLiteral("center-em") || s == QStringLiteral("em")) {
        return ffgui::GlyphPlacementMode::CenterInEm;
    }
    if (s == QStringLiteral("raw") || s == QStringLiteral("original")) {
        return ffgui::GlyphPlacementMode::OriginalSvgCoords;
    }
    return ffgui::GlyphPlacementMode::AutoAspectRatio;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("FontForgeGUI"));
    app.setApplicationName(QStringLiteral("FontForgeGUI"));
    app.setApplicationVersion(QStringLiteral("0.5.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Modern Qt 6 GUI and CLI tool for FontForge"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption inputOpt({QStringLiteral("i"), QStringLiteral("input")},
                                QStringLiteral("Input font file (.ttf, .otf, .sfd, .woff2, etc.). If omitted and --output is specified, a new blank font will be created."),
                                QStringLiteral("file"));
    QCommandLineOption outputOpt({QStringLiteral("o"), QStringLiteral("output")},
                                 QStringLiteral("Output font file path to export/save the font."),
                                 QStringLiteral("file"));
    QCommandLineOption startUnicodeOpt({QStringLiteral("u"), QStringLiteral("start-unicode")},
                                       QStringLiteral("Starting Unicode code point for imported glyphs (e.g. U+F0000 or 0xF0000). Defaults to U+F0000."),
                                       QStringLiteral("unicode"), QStringLiteral("U+F0000"));
    QCommandLineOption importImagesOpt({QStringLiteral("m"), QStringLiteral("import-images")},
                                       QStringLiteral("Directory or comma-separated list of SVG/PNG files to import."),
                                       QStringLiteral("path"));
    QCommandLineOption importDefOpt({QStringLiteral("d"), QStringLiteral("import-def")},
                                    QStringLiteral("JSON file containing batch import definitions (glyphs, unicodes, options)."),
                                    QStringLiteral("file.json"));
    QCommandLineOption placementOpt(QStringLiteral("placement"),
                                    QStringLiteral("Placement mode: auto, baseline, center-ascent, center-em, raw. Defaults to auto."),
                                    QStringLiteral("mode"), QStringLiteral("auto"));
    QCommandLineOption scaleFactorOpt(QStringLiteral("scale-factor"),
                                      QStringLiteral("Scale multiplier for imported glyphs (e.g. 1.0 = 100%)."),
                                      QStringLiteral("factor"), QStringLiteral("1.0"));
    QCommandLineOption overwriteOpt(QStringLiteral("overwrite"),
                                    QStringLiteral("Overwrite existing glyph code points if already present."));
    QCommandLineOption headlessOpt(QStringLiteral("headless"),
                                   QStringLiteral("Force headless batch execution without GUI window."));
    QCommandLineOption guiOpt(QStringLiteral("gui"),
                              QStringLiteral("Force GUI window even if batch parameters are given."));

    parser.addOption(inputOpt);
    parser.addOption(outputOpt);
    parser.addOption(startUnicodeOpt);
    parser.addOption(importImagesOpt);
    parser.addOption(importDefOpt);
    parser.addOption(placementOpt);
    parser.addOption(scaleFactorOpt);
    parser.addOption(overwriteOpt);
    parser.addOption(headlessOpt);
    parser.addOption(guiOpt);

    parser.addPositionalArgument(QStringLiteral("font"),
                                 QStringLiteral("Optional font file to open on startup (equivalent to --input)."));
    parser.process(app);

    QString inputFont = parser.value(inputOpt);
    const QString outputFont = parser.value(outputOpt);
    const QString importImages = parser.value(importImagesOpt);
    const QString importDef = parser.value(importDefOpt);
    const bool forceHeadless = parser.isSet(headlessOpt);
    const bool forceGui = parser.isSet(guiOpt);

    const QStringList posArgs = parser.positionalArguments();
    if (inputFont.isEmpty() && !posArgs.isEmpty()) {
        inputFont = posArgs.first();
    }

    const bool hasBatchJob = (!importImages.isEmpty() || !importDef.isEmpty());
    const bool runHeadless = forceHeadless || (hasBatchJob && !outputFont.isEmpty() && !forceGui);

    if (runHeadless) {
        std::cout << "FontForgeGUI v0.5.0 (Headless Batch Mode)\n";
        ffgui::FontForgeBridge bridge;
        bridge.initialize();

        ffgui::FontDocumentSnapshot snapshot;
        QString err;

        if (!inputFont.isEmpty() && QFileInfo::exists(inputFont)) {
            std::cout << "Loading font: " << inputFont.toStdString() << "\n";
            if (!bridge.openFont(inputFont, &snapshot, &err)) {
                std::cerr << "Error loading font: " << err.toStdString() << "\n";
                return 1;
            }
        } else {
            std::cout << "Creating new blank font...\n";
            ffgui::NewFontOptions newOpts;
            if (!outputFont.isEmpty()) {
                newOpts.fontName = QFileInfo(outputFont).completeBaseName();
                newOpts.familyName = newOpts.fontName;
            }
            if (!bridge.createNewFont(newOpts, &snapshot, &err)) {
                std::cerr << "Error creating font: " << err.toStdString() << "\n";
                return 1;
            }
        }

        if (hasBatchJob) {
            ffgui::BatchImportJob job;
            if (!importDef.isEmpty()) {
                std::cout << "Reading batch import definition: " << importDef.toStdString() << "\n";
                if (!ffgui::BatchImportDefinition::loadFromFile(importDef, &job, &err)) {
                    std::cerr << "Error loading JSON definition: " << err.toStdString() << "\n";
                    return 1;
                }
            } else if (!importImages.isEmpty()) {
                const int32_t startCp = parseCliUnicode(parser.value(startUnicodeOpt));
                if (startCp < 0) {
                    std::cerr << "Invalid start unicode: " << parser.value(startUnicodeOpt).toStdString() << "\n";
                    return 1;
                }

                QFileInfo imgFi(importImages);
                QStringList filePaths;
                if (imgFi.isDir()) {
                    QDir d(importImages);
                    const QStringList entries = d.entryList({QStringLiteral("*.svg"), QStringLiteral("*.png")},
                                                            QDir::Files, QDir::Name);
                    for (const auto& e : entries) {
                        filePaths.append(d.filePath(e));
                    }
                } else {
                    filePaths = importImages.split(QLatin1Char(','), Qt::SkipEmptyParts);
                }

                int32_t currentCp = startCp;
                for (const auto& fp : filePaths) {
                    ffgui::BatchImportItem item;
                    item.filePath = fp.trimmed();
                    item.unicode = currentCp++;
                    job.items.append(item);
                }

                job.overwriteExisting = parser.isSet(overwriteOpt);
                job.defaultOptions.placementMode = parseCliPlacement(parser.value(placementOpt));
                job.defaultOptions.scaleFactor = parser.value(scaleFactorOpt).toDouble();
                if (job.defaultOptions.scaleFactor <= 0.01) {
                    job.defaultOptions.scaleFactor = 1.0;
                }
            }

            std::cout << "Importing " << job.items.size() << " glyph(s)...\n";
            ffgui::BatchImportResult bRes;
            if (!bridge.batchImport(job, &bRes, &err)) {
                std::cerr << "Batch import failed: " << err.toStdString() << "\n";
                return 1;
            }

            std::cout << "Imported: " << bRes.importedCount << ", Skipped: " << bRes.skippedCount << "\n";
            for (const auto& e : bRes.errors) {
                std::cerr << "  - " << e.toStdString() << "\n";
            }
        }

        if (!outputFont.isEmpty()) {
            std::cout << "Exporting font to: " << outputFont.toStdString() << "\n";
            ffgui::ExportOptions expOpts;
            expOpts.outputPath = outputFont;
            if (!bridge.exportFont(expOpts, &err)) {
                std::cerr << "Export failed: " << err.toStdString() << "\n";
                return 1;
            }
            std::cout << "Successfully saved font!\n";
        }

        return 0;
    }

    // Interactive GUI mode
    ffgui::MainWindow window;
    window.show();

    if (!inputFont.isEmpty() && QFileInfo::exists(inputFont)) {
        window.openFontPath(inputFont);
    } else if (!outputFont.isEmpty() && hasBatchJob) {
        ffgui::NewFontOptions newOpts;
        newOpts.fontName = QFileInfo(outputFont).completeBaseName();
        newOpts.familyName = newOpts.fontName;
        window.createNewFont(newOpts);
    }

    if (hasBatchJob) {
        if (!importDef.isEmpty()) {
            window.openBatchImport({}, {}, importDef);
        } else if (!importImages.isEmpty()) {
            QFileInfo imgFi(importImages);
            if (imgFi.isDir()) {
                window.openBatchImport({}, importImages, {});
            } else {
                window.openBatchImport(importImages.split(QLatin1Char(','), Qt::SkipEmptyParts), {}, {});
            }
        }
    }

    return app.exec();
}
