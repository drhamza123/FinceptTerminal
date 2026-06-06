// MT5FleetTradeTab.h
#pragma once
#include <QWidget>
class QLabel; class QPushButton; class QTableWidget;
namespace fincept::screens {
class MT5FleetTradeTab : public QWidget {
  public: explicit MT5FleetTradeTab(QWidget* parent = nullptr);
  private: void build_ui(); void apply_theme();
  QPushButton* kill_btn_ = nullptr; QTableWidget* pos_table_ = nullptr;
};
} // namespace
