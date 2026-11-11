#include "backend/FontWorker.hpp"

#include "backend/FontForgeBridge.hpp"

namespace ffgui {

FontWorker::FontWorker(QObject* parent)
    : QObject(parent), m_bridge(std::make_unique<FontForgeBridge>()) {
    m_bridge->setLogCallback([this](const QString& level, const QString& msg) {
        emit logMessage(level, msg);
    });
    m_bridge->setProgressCallback([this](int current, int total, const QString& status) {
        emit progressUpdated(current, total, status);
    });
}

FontWorker::~FontWorker() {
    if (m_bridge) {
        m_bridge->closeFont();
    }
}

void FontWorker::initialize() {
    m_bridge->initialize();
}

void FontWorker::openFont(const QString& path) {
    FontDocumentSnapshot snapshot;
    QString error;
    if (m_bridge->openFont(path, &snapshot, &error)) {
        emit fontLoaded(snapshot);
    } else {
        emit fontLoadFailed(error);
    }
}

void FontWorker::exportFont(const ExportOptions& options) {
    QString error;
    if (m_bridge->exportFont(options, &error)) {
        emit fontExported(options.outputPath);
    } else {
        emit fontExportFailed(error);
    }
}

void FontWorker::closeFont() {
    m_bridge->closeFont();
    emit fontClosed();
}

} // namespace ffgui

