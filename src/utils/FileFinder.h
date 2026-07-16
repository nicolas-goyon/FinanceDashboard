#pragma once

#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>

// ── FileFinder — locate data files on the filesystem ─────────────────────────
//
// All functions are header-only and stateless.
//
// Typical usage:
//
//   // Resolve a file from a list of candidate locations
//   QString path = FileFinder::findFirst({
//       QDir::homePath() + "/data/portfolio.json",
//       QCoreApplication::applicationDirPath() + "/data/portfolio.json",
//       "C:/MyData/portfolio.json"
//   });
//
//   // Enumerate all JSON files under a directory
//   QStringList files = FileFinder::findFiles("C:/MyData", "*.json", /*recursive=*/true);

namespace FileFinder {

/// Returns true if `path` points to an existing, readable file.
inline bool exists(const QString& path)
{
    const QFileInfo fi(path);
    return fi.exists() && fi.isFile() && fi.isReadable();
}

/// Returns the first path from `candidates` that exists and is readable.
/// Returns an empty QString if none qualify.
inline QString findFirst(const QStringList& candidates)
{
    for (const QString& p : candidates) {
        if (exists(p))
            return p;
    }
    return {};
}

/// Returns absolute paths of all readable files inside `directory` whose
/// names match `nameFilter` (shell wildcard, e.g. "*.json").
/// Set `recursive` to true to descend into sub-directories.
inline QStringList findFiles(const QString& directory,
                              const QString& nameFilter = QStringLiteral("*"),
                              bool recursive = false)
{
    const QDir dir(directory);
    if (!dir.exists())
        return {};

    QStringList results;

    // Files in this directory
    const auto entries = dir.entryInfoList(
        QStringList{nameFilter},
        QDir::Files | QDir::Readable,
        QDir::Name
    );
    for (const QFileInfo& fi : entries)
        results << fi.absoluteFilePath();

    // Recurse into sub-directories if requested
    if (recursive) {
        const auto subdirs = dir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable
        );
        for (const QFileInfo& sub : subdirs)
            results << findFiles(sub.absoluteFilePath(), nameFilter, /*recursive=*/true);
    }

    return results;
}

} // namespace FileFinder
