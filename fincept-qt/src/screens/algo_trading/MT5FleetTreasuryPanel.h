// MT5FleetTreasuryPanel.h — copied from TreasuryPanel, renamed for MT5
#pragma once
#include <QString>
#include <QWidget>
class QLabel; class QPushButton;
namespace fincept::screens {
class MT5FleetTreasuryPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetTreasuryPanel(QWidget* parent = nullptr);
    ~MT5FleetTreasuryPanel() override;
  private:
    void build_ui(); void apply_theme();
    QLabel* status_pill_ = nullptr;
    QLabel* total_balance_ = nullptr;
    QLabel* total_pnl_ = nullptr;
    QLabel* total_equity_ = nullptr;
    QLabel* running_eas_ = nullptr;
    QPushButton* refresh_button_ = nullptr;
};
} // namespace
