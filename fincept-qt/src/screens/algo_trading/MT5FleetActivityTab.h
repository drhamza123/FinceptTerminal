// MT5FleetActivityTab.h
#pragma once
#include <QWidget>
class QTableWidget;
namespace fincept::screens {
class MT5FleetActivityTab : public QWidget {
  public: explicit MT5FleetActivityTab(QWidget* parent = nullptr);
  private: void build_ui();
  QTableWidget* log_table_ = nullptr;
};
} // namespace
