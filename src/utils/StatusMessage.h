#ifndef FINANCEDASHBOARD_UTILS_STATUSMESSAGE_H
#define FINANCEDASHBOARD_UTILS_STATUSMESSAGE_H

#include <QObject>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <functional>

// ── StatusMessage ────────────────────────────────────────────────────────────
// The status line of a controller: a text, its level, and one timer that
// clears it (#54).
//
// Each controller used to start a QTimer::singleShot for each message, so the
// timer of an older message cleared a newer one too early (a warning shown
// 2.3 s after a message vanished after 0.7 s). Here a new message restarts the
// one timer: each message stays for the whole delay. The level gives the
// StatusBadge variant, so that a failure is not shown in green. A failure
// (Danger) stays until the next message: the user must not miss it (#39).
//
// The controller has one as a member, and exposes `statusText` and
// `statusLevel` (NOTIFY statusChanged) from it.
class StatusMessage
{
public:
    enum class Level : std::uint8_t { Success, Warning, Danger };

    // `changed` runs each time the text or the level changes: the controller
    // emits its statusChanged() there. `owner` is the controller that has
    // this member; it is the context of the timer connection.
    StatusMessage(QObject* owner, int clearAfterMs, std::function<void()> changed);

    // Success and Warning go after the delay; Danger stays until the next
    // message.
    void show(Level level, const QString& text);

    [[nodiscard]] QString text() const { return m_text; }
    // "success", "warning" or "danger": a StatusBadge variant.
    [[nodiscard]] QString variant() const;

private:
    QTimer m_timer;
    std::function<void()> m_changed;
    QString m_text;
    Level m_level{Level::Success};
};

#endif // FINANCEDASHBOARD_UTILS_STATUSMESSAGE_H
