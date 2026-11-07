#include "controllers/ui/Language.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>
#include <QVariantMap>

namespace {

const QString kSettingsKey = QStringLiteral("ui/language");
const QString kSystem = QStringLiteral("system");
const QString kEnglish = QStringLiteral("en");
const QString kFrench = QStringLiteral("fr");

} // namespace

Language::Language() = default;

Language& Language::instance()
{
    static Language* language = new Language(); // lives as long as the app; QML does not own it
    return *language;
}

Language* Language::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(jsEngine)
    Language& language = instance();
    language.m_engine = qmlEngine;
    QJSEngine::setObjectOwnership(&language, QJSEngine::CppOwnership);
    return &language;
}

void Language::applyAtStartup()
{
    const QString saved = QSettings().value(kSettingsKey, kSystem).toString();
    instance().install(saved == kEnglish || saved == kFrench ? saved : kSystem);
}

QString Language::resolve(const QString& language)
{
    if (language == kEnglish || language == kFrench) {
        return language;
    }
    // "system": French when the computer is in French, else English.
    return QLocale::system().language() == QLocale::French ? kFrench : kEnglish;
}

void Language::setLanguage(const QString& language)
{
    const QString value = language == kEnglish || language == kFrench ? language : kSystem;
    if (value == m_language) {
        return;
    }
    QSettings().setValue(kSettingsKey, value);
    install(value);
    if (m_engine) {
        m_engine->retranslate();
    }
}

void Language::install(const QString& language)
{
    m_language = language;
    m_effective = resolve(language);

    for (auto* translator : {&m_appTranslator, &m_qtBaseTranslator, &m_qtQuickTranslator}) {
        if (*translator) {
            QCoreApplication::removeTranslator(translator->get());
            translator->reset();
        }
    }
    // The dates and numbers of the pages (QLocale()) follow the language.
    QLocale::setDefault(m_effective == kFrench ? QLocale(QLocale::French, QLocale::France)
                                               : QLocale(QLocale::English, QLocale::UnitedStates));

    if (m_effective != kEnglish) { // English is the language of the sources
        const QString qtPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        const auto load = [](std::unique_ptr<QTranslator>& slot, const QString& name, const QString& dir) {
            auto translator = std::make_unique<QTranslator>();
            if (translator->load(name, dir) && QCoreApplication::installTranslator(translator.get())) {
                slot = std::move(translator);
            }
        };
        load(m_qtBaseTranslator, QStringLiteral("qtbase_") + m_effective, qtPath);
        load(m_qtQuickTranslator, QStringLiteral("qtdeclarative_") + m_effective, qtPath);
        load(m_appTranslator, QStringLiteral("financedashboard_") + m_effective, QStringLiteral(":/i18n"));
        if (!m_appTranslator) {
            qWarning().noquote() << "[Language] no translation for" << m_effective;
        }
    }
    emit languageChanged();
}

QVariantList Language::choices() const
{
    return {
        QVariantMap{{"code", kSystem}, {"name", QCoreApplication::translate("Language", "System language")}},
        QVariantMap{{"code", kEnglish}, {"name", QStringLiteral("English")}},
        QVariantMap{{"code", kFrench}, {"name", QStringLiteral("Français")}},
    };
}
