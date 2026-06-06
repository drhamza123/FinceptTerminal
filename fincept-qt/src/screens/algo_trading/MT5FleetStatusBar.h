// src/screens/algo_trading/MT5FleetStatusBar.h
#pragma once
#include "screens/algo_trading/MT5FleetTypes.h"
#include <QLabel>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

class MT5FleetStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetStatusBar(QWidget* parent = nullptr);
    void set_summary(const mt5::EASummary& s);
    void start_clock();

  private:
    void build_ui();
    QLabel* brand_ = nullptr; QLabel* engine_ = nullptr;
    QLabel* connected_ = nullptr; QLabel* pnl_ = nullptr; QLabel* time_ = nullptr;
    QTimer* timer_ = nullptr;
};

} // namespace fincept::screens
