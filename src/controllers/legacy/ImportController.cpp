#include "ImportController.h"
#include "models/PortfolioData.h"
#include "utils/FileFinder.h"
#include "utils/Formatter.h"
#include "utils/JsonReader.h"

#include <QFileInfo>

namespace {

constexpr int kMaxPreview = 200;

QVariantMap row(const QString& key, const QString& value,
                const QString& color = QStringLiteral("#cdd6f4"))
{
    return { { "key", key }, { "value", value }, { "color", color } };
}

} // namespace

ImportController::ImportController(QObject* parent)
    : QObject(parent)
{
}

QString ImportController::localPath(const QUrl& url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

void ImportController::importFile(const QString& rawPath)
{
    const QString path = rawPath.trimmed();

    if (path.isEmpty()) {
        setFailure(QStringLiteral("⚠  No path specified."),
                   QStringLiteral("Please enter or browse to a file path."));
        return;
    }

    if (!FileFinder::exists(path)) {
        setFailure(QStringLiteral("⚠  File not found."),
                   QStringLiteral("File not found:\n") + path);
        return;
    }

    const auto result = JsonReader::read<Data::PortfolioSummary>(path);
    if (!result) {
        setFailure(QStringLiteral("⚠  Parse error — check the application log for details."),
                   QStringLiteral("Failed to parse the file.\n"
                                  "Make sure it is a valid portfolio JSON."));
        return;
    }

    const Data::PortfolioSummary& s = *result;
    const bool perfPositive  = s.performance >= 0;
    const bool gainsPositive = s.gains >= 0;
    const char* kGreen = "#a6e3a1";
    const char* kRed   = "#f38ba8";

    m_summaryRows = {
        row("Timeframe",       s.timeframe.isEmpty() ? QStringLiteral("—") : s.timeframe),
        row("Portfolio Value", Fmt::money(s.overallPortfolioAmount)),
        row("Performance",
            (perfPositive ? QStringLiteral("+") : QString())
                + QString::number(s.performance, 'f', 2) + QStringLiteral(" %"),
            perfPositive ? kGreen : kRed),
        row("Gains",           Fmt::money(s.gains, /*showPlus=*/true),
            gainsPositive ? kGreen : kRed),
        row("Dividends",       Fmt::money(s.dividends)),
        row("Buy",  QString("%1  (%2 trade%3)")
                        .arg(Fmt::money(s.buyAmount)).arg(s.buyCount)
                        .arg(s.buyCount == 1 ? "" : "s")),
        row("Sell", QString("%1  (%2 trade%3)")
                        .arg(Fmt::money(s.sellAmount)).arg(s.sellCount)
                        .arg(s.sellCount == 1 ? "" : "s")),
        row("Best Asset",      s.bestAsset.isEmpty() ? QStringLiteral("—") : s.bestAsset),
    };

    m_points.clear();
    m_totalPoints = s.data.size();
    const int previewCount = qMin(m_totalPoints, kMaxPreview);
    for (int i = 0; i < previewCount; ++i)
        m_points.append(QVariantMap{
            { "time",  s.data[i].time },
            { "value", Fmt::money(s.data[i].value) },
        });

    m_capNote = m_totalPoints > kMaxPreview
        ? QString("… and %1 more points (preview capped at %2).")
              .arg(m_totalPoints - kMaxPreview).arg(kMaxPreview)
        : QString();

    m_hasResult   = true;
    m_message.clear();
    m_statusState = QStringLiteral("ok");
    m_statusText  = QString("✓  Imported: %1   (%2 data points)")
                        .arg(QFileInfo(path).fileName())
                        .arg(m_totalPoints);
    emit resultChanged();
}

void ImportController::setFailure(const QString& status, const QString& message)
{
    m_hasResult   = false;
    m_summaryRows.clear();
    m_points.clear();
    m_totalPoints = 0;
    m_capNote.clear();
    m_statusState = QStringLiteral("error");
    m_statusText  = status;
    m_message     = message;
    emit resultChanged();
}
