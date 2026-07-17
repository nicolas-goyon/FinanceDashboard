#ifndef FINANCEDASHBOARD_MODELS_IMPORTLOG_H
#define FINANCEDASHBOARD_MODELS_IMPORTLOG_H

#include <QString>

#include <cstdint>

// ── Data::ImportLogEntry ─────────────────────────────────────────────────────
//
// One line in the "console/log" a statement-import template can attach to a
// parse run. Header-only, plain data — no persistence, no QML exposure of
// its own (SnapshotImportController::logEntries() turns these into
// QVariantMaps for the QML review page).

namespace Data {

enum class LogLevel : std::uint8_t { Info, Warning, Error };

struct ImportLogEntry {
    LogLevel level{LogLevel::Info};
    int      lineNumber{0};   // 1-based source line, 0 if not tied to a line
    QString  message;
    QString  rawLine;         // original line text, empty if not applicable

    static QString levelLabel(LogLevel lvl)
    {
        switch (lvl) {
            case LogLevel::Error:   return QStringLiteral("Error");
            case LogLevel::Warning: return QStringLiteral("Warning");
            case LogLevel::Info:    return QStringLiteral("Info");
        }
        return QStringLiteral("Info");
    }
};

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_IMPORTLOG_H
