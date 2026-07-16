#pragma once

#include <optional>
#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

// ── JsonReader — generic file-based JSON deserializer ────────────────────────
//
// Reads a JSON file and deserializes it into any type T that satisfies the
// contract:
//
//     static T T::fromJson(const QJsonObject& obj);
//
// Returns std::nullopt on any error (file not found, malformed JSON, etc.)
// and prints a qWarning with the reason.
//
// Usage:
//
//   auto result = JsonReader::read<PortfolioSummary>("/path/to/file.json");
//   if (!result) { /* handle error */ }
//   const PortfolioSummary& summary = *result;
//
// To make a new type readable, add a static fromJson() to its declaration:
//
//   struct MyData {
//       int foo;
//       static MyData fromJson(const QJsonObject& obj) {
//           return { obj.value("foo").toInt() };
//       }
//   };

namespace JsonReader {

template<typename T>
std::optional<T> read(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[JsonReader] Cannot open file:" << filePath;
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[JsonReader] Parse error in" << filePath
                   << "at offset" << parseError.offset
                   << "—" << parseError.errorString();
        return std::nullopt;
    }

    if (!doc.isObject()) {
        qWarning() << "[JsonReader] Root element is not a JSON object in" << filePath;
        return std::nullopt;
    }

    return T::fromJson(doc.object());
}

} // namespace JsonReader
