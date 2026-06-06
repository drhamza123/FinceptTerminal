// MT5FleetHoldingsBar.h — EA fleet status bar (copied from HoldingsBar)
#pragma once
#include <QLabel>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

class MT5FleetHoldingsBar : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetHoldingsBar(QWidget* parent = nullptr);
    ~MT5FleetHoldingsBar() override;
  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
  private:
    void build_ui();
    void apply_theme();
    void refresh();
    QLabel* ea_count_ = nullptr;
    QLabel* total_balance_ = nullptr;
    QLabel* total_pnl_ = nullptr;
    QLabel* win_rate_ = nullptr;
    QLabel* running_ = nullptr;
    QLabel* updated_value_ = nullptr;
    QLabel* feed_status_ = nullptr;
    QTimer* refresh_timer_ = nullptr;
};

} // namespace fincept::screens
