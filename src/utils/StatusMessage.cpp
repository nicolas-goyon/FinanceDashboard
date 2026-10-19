#include "StatusMessage.h"

#include <utility>

StatusMessage::StatusMessage(QObject* owner, int clearAfterMs, std::function<void()> changed)
    : m_changed(std::move(changed))
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(clearAfterMs);
    QObject::connect(&m_timer, &QTimer::timeout, owner, [this]() {
        m_text.clear();
        m_changed();
    });
}

void StatusMessage::show(Level level, const QString& text)
{
    m_level = level;
    m_text = text;
    if (level == Level::Danger) {
        m_timer.stop();  // a failure stays until the next message (#39)
    } else {
        m_timer.start(); // restarts it when it runs: the new message gets the whole delay
    }
    m_changed();
}

QString StatusMessage::variant() const
{
    switch (m_level) {
        case Level::Warning: return QStringLiteral("warning");
        case Level::Danger:  return QStringLiteral("danger");
        case Level::Success: break;
    }
    return QStringLiteral("success");
}
