#ifndef FINANCEDASHBOARD_MODELS_CATEGORYRULE_H
#define FINANCEDASHBOARD_MODELS_CATEGORYRULE_H

#include <QString>

// ── Data::CategoryRule ────────────────────────────────────────────────────────
//
// A user-authored "always categorize labels like X as Y" rule — the top rung
// of the category-prediction cascade (see docs/category-prediction.md and
// src/predict/CategoryPredictor.h). Mirrors the row shape of the
// `category_rules` table (db/tables/CategoryRulesTable.h).
//
// `pattern` is a **normalized merchant key**, not a raw label and not a
// regex: it is whatever Predict::LabelNormalizer::normalize() produced for
// the transaction the user created the rule from. Matching is
// "incoming key equals the pattern, or contains it as a whole-token run" —
// deliberately not a regex, so the rule list stays something the user can
// read and edit in a table without learning a pattern language.
//
// This is the only part of the predictor that is persisted. Everything else
// (the exact/fuzzy/similar rungs) is derived from the already-categorized
// rows in the `transactions` table and rebuilt on demand — see
// CategoryPredictor::train().

namespace Data {

struct CategoryRule {
    int     id = -1;
    QString pattern;      // normalized merchant key, e.g. "CARREFOUR MARKET"
    QString category;
    QString subcategory;  // may be empty — "category only" is a valid rule
    int     priority = 0; // higher wins; ties broken by longer pattern first
};

} // namespace Data

#endif // FINANCEDASHBOARD_MODELS_CATEGORYRULE_H
