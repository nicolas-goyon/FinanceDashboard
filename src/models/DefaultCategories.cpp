#include "models/DefaultCategories.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cmath>

namespace Data {

namespace {

constexpr int kShadeStep = 11; // HSL lightness step between two shades (0-255)
// The lightness range of a shade (0-255): readable on the dark background,
// and not white.
constexpr double kMinLightness = 95.0;
constexpr double kMaxLightness = 215.0;

struct Seed {
    const char* name;
    const char* color;
    std::initializer_list<const char*> subcategories;
};

// QT_TRANSLATE_NOOP marks the names for lupdate; they are translated in
// defaultCategories().
const std::array<Seed, 15> kSeed = {{
    { QT_TRANSLATE_NOOP("DefaultCategories", "Income"), "#a6e3a1", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Salary"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Bonus"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Freelance"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Social benefits"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Refunds"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Interest"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Dividends"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Gifts received"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Housing"), "#b4befe", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Rent"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Mortgage"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Charges"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Home insurance"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Furniture"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Repairs"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Groceries"), "#94e2d5", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Supermarket"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Market"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Bakery"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Utilities"), "#f9e2af", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Electricity"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Gas"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Water"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Internet"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Mobile phone"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Subscriptions"), "#f5c2e7", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Streaming"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Music"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Software and cloud"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Press"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Sport and gym"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Transport"), "#89dceb", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Public transit"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Fuel"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Rideshare"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Train and plane"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Car insurance"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Car maintenance"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Parking and tolls"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Health"), "#f38ba8", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Doctor"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Pharmacy"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Dentist and optician"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Health insurance"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Leisure"), "#cba6f7", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Restaurants"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Bars and cafes"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Cinema and shows"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Hobbies"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Holidays"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Shopping"), "#fab387", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Clothes"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Electronics"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Home and garden"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Personal care"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Children and education"), "#eba0ac", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Childcare"),
        QT_TRANSLATE_NOOP("DefaultCategories", "School"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Courses"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Pocket money"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Investments"), "#74c7ec", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Stocks and ETFs"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Life insurance"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Retirement savings"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Real estate"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Crypto"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Transfers"), "#89b4fa", {
        QT_TRANSLATE_NOOP("DefaultCategories", "To my savings"),
        QT_TRANSLATE_NOOP("DefaultCategories", "To another person"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Cash withdrawal"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Taxes and fees"), "#f2cdcd", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Income tax"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Property tax"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Bank fees"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Fines"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Gifts and donations"), "#f5e0dc", {
        QT_TRANSLATE_NOOP("DefaultCategories", "Gifts"),
        QT_TRANSLATE_NOOP("DefaultCategories", "Donations"),
    } },
    { QT_TRANSLATE_NOOP("DefaultCategories", "Other"), "#6c7086", {} },
}};

// Shade `index` of `color` ("#rrggbb"): 0 is the colour itself, then darker,
// lighter, darker again… (one kShadeStep of HSL lightness each time).
// Plain arithmetic: the core sources do not link Qt Gui (QColor).
QString shade(const QString& color, int index)
{
    if (index == 0 || color.size() != 7) {
        return color;
    }
    bool ok = false;
    const uint rgb = color.mid(1).toUInt(&ok, 16);
    if (!ok) {
        return color;
    }
    const double r = ((rgb >> 16) & 0xff) / 255.0;
    const double g = ((rgb >> 8) & 0xff) / 255.0;
    const double b = (rgb & 0xff) / 255.0;
    const double maxC = std::max({r, g, b});
    const double minC = std::min({r, g, b});
    double h = 0.0;
    double sat = 0.0;
    const double l = (maxC + minC) / 2.0;
    const double d = maxC - minC;
    if (d > 0.0) {
        sat = l > 0.5 ? d / (2.0 - maxC - minC) : d / (maxC + minC);
        if (maxC == r) {
            h = (g - b) / d + (g < b ? 6.0 : 0.0);
        } else if (maxC == g) {
            h = (b - r) / d + 2.0;
        } else {
            h = (r - g) / d + 4.0;
        }
        h /= 6.0;
    }
    // The offsets -1, +1, -2, +2… steps, without the ones that leave the
    // range [kMinLightness, kMaxLightness]: a light colour gets more dark
    // shades, and no two shades are the same.
    const double base = l * 255.0;
    double newL = l;
    int found = 0;
    for (int k = 1; k <= 2 * 255 / kShadeStep && found < index; ++k) {
        const double candidate = base + ((k % 2 == 1) ? -1 : 1) * ((k + 1) / 2) * kShadeStep;
        if (candidate >= kMinLightness && candidate <= kMaxLightness) {
            ++found;
            newL = candidate / 255.0;
        }
    }

    const auto hueToRgb = [](double p, double q, double t) {
        if (t < 0.0) { t += 1.0; }
        if (t > 1.0) { t -= 1.0; }
        if (t < 1.0 / 6.0) { return p + (q - p) * 6.0 * t; }
        if (t < 1.0 / 2.0) { return q; }
        if (t < 2.0 / 3.0) { return p + (q - p) * (2.0 / 3.0 - t) * 6.0; }
        return p;
    };
    double nr = newL;
    double ng = newL;
    double nb = newL;
    if (sat > 0.0) {
        const double q = newL < 0.5 ? newL * (1.0 + sat) : newL + sat - newL * sat;
        const double p = 2.0 * newL - q;
        nr = hueToRgb(p, q, h + 1.0 / 3.0);
        ng = hueToRgb(p, q, h);
        nb = hueToRgb(p, q, h - 1.0 / 3.0);
    }
    const auto byte = [](double v) { return static_cast<uint>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
    return QStringLiteral("#%1").arg((byte(nr) << 16) | (byte(ng) << 8) | byte(nb), 6, 16, QLatin1Char('0'));
}

QString tr(const char* text)
{
    return QCoreApplication::translate("DefaultCategories", text);
}

} // namespace

QVector<DefaultCategory> defaultCategories()
{
    QVector<DefaultCategory> result;
    result.reserve(static_cast<qsizetype>(kSeed.size()));
    for (const Seed& seed : kSeed) {
        DefaultCategory category{tr(seed.name), QString::fromLatin1(seed.color), {}};
        int index = 0;
        for (const char* sub : seed.subcategories) {
            category.subcategories.append(DefaultSubcategory{tr(sub), shade(category.color, index++)});
        }
        result.append(category);
    }
    return result;
}

} // namespace Data
