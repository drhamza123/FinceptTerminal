// v037_seed_guardian_agent.cpp — Seed the Guardian Agent into agent_configs.

#include "core/result/Result.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QSqlDatabase>
#include <QSqlQuery>

namespace fincept {
namespace {

static Result<void> apply_v037(QSqlDatabase& db) {
    auto sql = [](QSqlDatabase& d, const QString& q) -> Result<void> {
        QSqlQuery qry(d);
        if (!qry.exec(q))
            return Result<void>::err(qPrintable(qry.lastError().text()));
        return Result<void>::ok();
    };

    // Insert Guardian Agent if not already present
    QSqlQuery check(db);
    check.prepare("SELECT COUNT(*) FROM agent_configs WHERE id = 'guardian_agent'");
    if (check.exec() && check.next() && check.value(0).toInt() == 0) {
        QSqlQuery ins(db);
        ins.prepare(
            "INSERT INTO agent_configs (id, name, description, config_json, category, is_default, is_active) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");
        ins.addBindValue("guardian_agent");
        ins.addBindValue("Guardian Agent");
        ins.addBindValue("Your primary AI financial analyst. Provides real-time market analysis, "
                          "FX Smart Reports, technical analysis, trade ideas, and portfolio insights.");
        ins.addBindValue(
            "{"
            "  \"system_prompt\": \"You are the Guardian Agent — an elite AI financial analyst embedded "
            "in the AI Stock Guardian terminal. You specialize in: (1) FX & commodity analysis with "
            "Entry/Target/Probability scenarios, (2) Technical analysis using EMA, MACD, AROON, "
            "Fibonacci, support/resistance, (3) Trade ideas with risk/reward ratios, (4) Portfolio "
            "optimization and risk management, (5) Real-time market commentary. Always provide "
            "actionable, data-driven insights. Format reports professionally with clear sections. "
            "When asked for FX Smart Reports, generate them in Equiti Securities style with letterhead, "
            "instrument analysis, scenarios, technical comments, tools used, and disclaimers.\","
            "  \"model\": \"gpt-4o-mini\","
            "  \"temperature\": 0.7,"
            "  \"max_tokens\": 4096,"
            "  \"tools_enabled\": true"
            "}");
        ins.addBindValue("ai");
        ins.addBindValue(1);  // is_default
        ins.addBindValue(1);  // is_active
        if (!ins.exec())
            return Result<void>::err(qPrintable(ins.lastError().text()));
    }

    return Result<void>::ok();
}

} // anonymous namespace

void register_migration_v037() {
    static bool done = false;
    if (done)
        return;
    done = true;
    MigrationRunner::register_migration({37, "seed_guardian_agent", apply_v037});
}

} // namespace fincept
