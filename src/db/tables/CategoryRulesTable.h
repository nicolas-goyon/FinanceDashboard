#ifndef FINANCEDASHBOARD_DB_TABLES_CATEGORYRULESTABLE_H
#define FINANCEDASHBOARD_DB_TABLES_CATEGORYRULESTABLE_H

#include "models/CategoryRule.h"

#include <QString>
#include <QVector>
#include <QSqlDatabase>

// ── CategoryRulesTable ────────────────────────────────────────────────────────
//
// Owns all SQL that touches the `category_rules` table — the user-authored
// "always categorize labels like X as Y" rules that form the top rung of the
// category-prediction cascade (see docs/category-prediction.md and
// src/predict/CategoryPredictor.h).
//
//   CREATE TABLE category_rules (
//     id          INTEGER PRIMARY KEY AUTOINCREMENT,
//     pattern     TEXT    NOT NULL UNIQUE,  -- a normalized merchant key
//     category    TEXT    NOT NULL,
//     subcategory TEXT    NOT NULL DEFAULT '',
//     priority    INTEGER NOT NULL DEFAULT 0
//   );
//
// **`pattern` is a normalized merchant key, never a raw label.** It is always
// whatever Predict::LabelNormalizer::normalize() produced — writers must
// normalize before calling insert(), or the rule will never match anything
// (the predictor only ever compares normalized keys). Same "both sides must
// use the one function" rule as AccountHashing::compute().
//
// `pattern` is UNIQUE so re-teaching the same merchant updates the existing
// rule instead of stacking a second, contradictory one — see
// Database::addCategoryRule(), which does the upsert.
//
// **This is the only predictor state that is persisted.** The exact/fuzzy/
// similar rungs are all derived from the already-categorized rows in the
// `transactions` table and rebuilt in memory on demand, so there is no model
// file, no schema for one, and nothing to migrate when the algorithm changes.
//
// Constructed with a connection name; looks up the live QSqlDatabase at call
// time, so it is safe to construct before Database::open() is called — same
// convention as every other table class here.

class CategoryRulesTable
{
public:
    explicit CategoryRulesTable(QString connectionName);

    // Create the table. Called once by Database::open().
    bool createTable();

    // Insert a new rule. Returns the new row id, or -1 on failure (including
    // the UNIQUE(pattern) violation). Callers that want "teach or re-teach"
    // semantics should use Database::addCategoryRule(), which checks for an
    // existing pattern first — same convention as CategoriesTable::insert().
    int insert(const Data::CategoryRule& rule);

    // Repoint an existing rule at a different category/subcategory, leaving
    // its pattern and priority alone. This is what "the user corrected a row
    // that a rule had already filled in" turns into.
    bool updateTarget(int ruleId, const QString& category, const QString& subcategory);

    bool remove(int ruleId);

    // Every rule, insertion order. Ordering into *match* order (priority,
    // then pattern specificity) is CategoryPredictor::train()'s job, not the
    // table's — see its sort comment.
    [[nodiscard]] QVector<Data::CategoryRule> all() const;

    // The rule whose pattern is exactly `pattern`, or a default-constructed
    // Data::CategoryRule (id == -1) if there is none. Exact match only — this
    // is the "does a rule for this merchant already exist" lookup, not the
    // predictor's token-run matching.
    [[nodiscard]] Data::CategoryRule byPattern(const QString& pattern) const;

private:
    [[nodiscard]] QSqlDatabase db() const;
    QString m_connectionName;
};

#endif // FINANCEDASHBOARD_DB_TABLES_CATEGORYRULESTABLE_H
