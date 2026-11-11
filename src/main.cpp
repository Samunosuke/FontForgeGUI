#include "ui/MainWindow.hpp"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("FontForgeGUI"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Modern Qt 6 GUI for FontForge"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("font"),
                                 QStringLiteral("Optional font file to open on startup."));
    parser.process(app);

    ffgui::MainWindow window;
    window.show();

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        window.openFontPath(args.first());
    }

    return app.exec();
}

