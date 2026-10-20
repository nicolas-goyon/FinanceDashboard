#include "update/SemVer.h"

#include <QChar>
#include <QLatin1Char>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <algorithm>

namespace Update {

namespace {

// https://semver.org/ (the regular expression of the specification, with an
// optional leading "v"). Numeric identifiers have no leading zero.
const QRegularExpression& versionPattern()
{
    static const QRegularExpression pattern(QStringLiteral(
        "^v?(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)"
        "(?:-((?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?"
        "(?:\\+[0-9a-zA-Z-]+(?:\\.[0-9a-zA-Z-]+)*)?$"));
    return pattern;
}

bool isNumeric(const QString& identifier)
{
    return !identifier.isEmpty() && std::all_of(identifier.cbegin(), identifier.cend(),
                                                [](QChar character) { return character.isDigit(); });
}

int compareIdentifiers(const QString& left, const QString& right)
{
    const bool leftNumeric = isNumeric(left);
    const bool rightNumeric = isNumeric(right);
    if (leftNumeric && rightNumeric) {
        // No leading zero (the pattern refuses them): a longer number is higher.
        if (left.size() != right.size()) {
            return left.size() < right.size() ? -1 : 1;
        }
        return left.compare(right);
    }
    if (leftNumeric != rightNumeric) {
        return leftNumeric ? -1 : 1; // a number is lower than a text
    }
    return left.compare(right); // ASCII order
}

int sign(int value)
{
    if (value > 0) {
        return 1;
    }
    return value < 0 ? -1 : 0;
}

} // namespace

SemVer parseSemVer(const QString& text)
{
    SemVer version;
    const QRegularExpressionMatch match = versionPattern().match(text.trimmed());
    if (!match.hasMatch()) {
        return version;
    }
    bool okMajor = false;
    bool okMinor = false;
    bool okPatch = false;
    version.majorVersion = match.captured(1).toInt(&okMajor);
    version.minorVersion = match.captured(2).toInt(&okMinor);
    version.patchVersion = match.captured(3).toInt(&okPatch);
    if (!okMajor || !okMinor || !okPatch) {
        return SemVer{}; // a number too large for an int
    }
    if (match.hasCaptured(4) && !match.captured(4).isEmpty()) {
        version.prerelease = match.captured(4).split(QLatin1Char('.'));
    }
    version.valid = true;
    return version;
}

int compareSemVer(const SemVer& left, const SemVer& right)
{
    if (left.majorVersion != right.majorVersion) {
        return left.majorVersion < right.majorVersion ? -1 : 1;
    }
    if (left.minorVersion != right.minorVersion) {
        return left.minorVersion < right.minorVersion ? -1 : 1;
    }
    if (left.patchVersion != right.patchVersion) {
        return left.patchVersion < right.patchVersion ? -1 : 1;
    }
    // A release is higher than its pre-releases.
    if (left.prerelease.isEmpty() || right.prerelease.isEmpty()) {
        if (left.prerelease.isEmpty() == right.prerelease.isEmpty()) {
            return 0;
        }
        return left.prerelease.isEmpty() ? 1 : -1;
    }
    const qsizetype common = std::min(left.prerelease.size(), right.prerelease.size());
    for (qsizetype i = 0; i < common; ++i) {
        const int result = compareIdentifiers(left.prerelease.at(i), right.prerelease.at(i));
        if (result != 0) {
            return sign(result);
        }
    }
    if (left.prerelease.size() == right.prerelease.size()) {
        return 0;
    }
    return left.prerelease.size() < right.prerelease.size() ? -1 : 1;
}

QString semVerText(const SemVer& version)
{
    QString text = QStringLiteral("%1.%2.%3").arg(version.majorVersion).arg(version.minorVersion).arg(version.patchVersion);
    if (!version.prerelease.isEmpty()) {
        text += QLatin1Char('-') + version.prerelease.join(QLatin1Char('.'));
    }
    return text;
}

} // namespace Update
