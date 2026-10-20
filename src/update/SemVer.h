#ifndef FINANCEDASHBOARD_UPDATE_SEMVER_H
#define FINANCEDASHBOARD_UPDATE_SEMVER_H

#include <QString>
#include <QStringList>

// ── Update::SemVer ───────────────────────────────────────────────────────────
//
// A version as the release channels write it (#129): MAJOR.MINOR.PATCH, then
// an optional pre-release part (0.2.2-exp.57, 0.2.2-exp.61.pr12, 0.2.2-rc.70),
// with an optional leading "v" (the tags). Build metadata ("+...") is read
// and ignored, as SemVer says.
//
// compareSemVer() follows the precedence of SemVer 2.0.0 (https://semver.org/ § 11):
// the numbers first, then a version without a pre-release part is higher than
// one with it, then the pre-release identifiers one by one (a number is lower
// than a text, numbers compare as numbers, texts in ASCII order, and more
// identifiers are higher when the first ones are equal). So
// 0.2.2-exp.57 < 0.2.2-exp.61.pr12 < 0.2.2-rc.70 < 0.2.2 < 0.2.3-exp.1.
//
// The updater (#117) compares the version of the app
// (QCoreApplication::applicationVersion()) with the versions of the releases.

namespace Update {

struct SemVer
{
    // Not "major" and "minor": glibc has macros with those names.
    int majorVersion = 0;
    int minorVersion = 0;
    int patchVersion = 0;
    QStringList prerelease; // {"exp", "57"} for 0.2.2-exp.57; empty for a release
    bool valid = false;     // false when the text is not a version
};

// "0.2.2", "v0.2.2-rc.70", "0.2.2-exp.61.pr12+build.5". An invalid text gives
// a SemVer with valid == false.
SemVer parseSemVer(const QString& text);

// < 0, 0 or > 0, as `left` is lower than, equal to or higher than `right`.
// Only for two valid versions.
int compareSemVer(const SemVer& left, const SemVer& right);

// "0.2.2-exp.57", without the "v" and the build metadata.
QString semVerText(const SemVer& version);

} // namespace Update

#endif // FINANCEDASHBOARD_UPDATE_SEMVER_H
