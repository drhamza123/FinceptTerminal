// src/screens/algo_trading/MT5FleetStatsRibbon.h
#pragma once
#include "screens/algo_trading/MT5FleetTypes.h"
#include <QLabel>
#include <QWidget>

namespace fincept::screens {

class MT5FleetStatsRibbon : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetStatsRibbon(QWidget* parent = nullptr);
    void set_summary(const mt5::EASummary& summary);

  private:
    struct HeroCell { QWidget* c = nullptr; QLabel* label = nullptr; QLabel* value = nullptr; QLabel* sub = nullptr; };
    QWidget* build_hero(HeroCell& cell, const QString& label_text, int value_px);
    void build_ui();

    HeroCell connected_; HeroCell pnl_; HeroCell winrate_; HeroCell trades_; HeroCell balance_; HeroCell today_;
};

} // namespace fincept::screens
