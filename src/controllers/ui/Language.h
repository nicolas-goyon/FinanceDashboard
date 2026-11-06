#ifndef FINANCEDASHBOARD_CONTROLLERS_UI_LANGUAGE_H
#define FINANCEDASHBOARD_CONTROLLERS_UI_LANGUAGE_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTranslator>
#include <QVariantList>
#include <QtQml/qqml.h>

#include <memory>

class QJSEngine;
class QQmlEngine;

// ── Language ─────────────────────────────────────────────────────────────────
//
// The language of the app (Settings page): "system" (the language of the
// computer, English when it is not one of ours), "en" or "fr". Kept in
// QSettings (key ui/language), so it is the same for every database of this
// computer.
//
// applyAtStartup() runs in main() before the QML loads: the default data that
// the controllers write on a new database (categories, #226) is then in that
// language. A change from the Settings page installs the translator of the
// new language and calls QQmlEngine::retranslate(): every qsTr() of the QML
// is read again at once. The texts that a controller makes in C++ (KPIs,
// tables) follow when the page reads its data again, at the next show.
//
// The translations are i18n/financedashboard_<code>.ts, built into the app
// by qt_add_translations() (resource prefix /i18n). The translations of Qt
// itself (qtbase, qtdeclarative) are loaded from the Qt installation when
// they are there.
class Language : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "system", "en" or "fr"
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    // The language in use: "en" or "fr" (the language of the computer for
    // "system").
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY languageChanged)
    // [{ code, name }] for the ComboBox of the Settings page; the names are
    // in their own language ("Français").
    Q_PROPERTY(QVariantList choices READ choices NOTIFY languageChanged)

public:
    static Language* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static Language& instance();

    // Installs the saved language (main(), before the QML).
    static void applyAtStartup();

    [[nodiscard]] QString language() const { return m_language; }
    void setLanguage(const QString& language);
    [[nodiscard]] QString effectiveLanguage() const { return m_effective; }
    [[nodiscard]] QVariantList choices() const;

signals:
    void languageChanged();

private:
    Language();
    void install(const QString& language);
    [[nodiscard]] static QString resolve(const QString& language);

    QString m_language = QStringLiteral("system");
    QString m_effective = QStringLiteral("en");
    std::unique_ptr<QTranslator> m_appTranslator;
    std::unique_ptr<QTranslator> m_qtBaseTranslator;
    std::unique_ptr<QTranslator> m_qtQuickTranslator;
    QPointer<QQmlEngine> m_engine;
};

#endif // FINANCEDASHBOARD_CONTROLLERS_UI_LANGUAGE_H
