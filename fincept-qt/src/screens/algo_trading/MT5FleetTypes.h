// MT5FleetTypes.h — Data types for MT5 Fleet View
#pragma once
#include <QString>
#include <QVector>
#include <QDateTime>

namespace fincept::mt5 {

struct EAInstance {
    QString eid;
    QString ea_name;
    int magic_number = 0;
    QString symbol;
    QString timeframe;
    QString status;
    double balance = 0;
    double equity = 0;
    double pnl = 0;
    QDateTime connected_at;
    QDateTime last_heartbeat_at;
};

struct EASummary {
    QVector<EAInstance> instances;
    int total_count = 0;
    int running_count = 0;
    double total_balance = 0;
    double total_equity = 0;
    double total_pnl = 0;
    double win_rate = 0;
    int total_trades = 0;
};

} // namespace fincept::mt5
