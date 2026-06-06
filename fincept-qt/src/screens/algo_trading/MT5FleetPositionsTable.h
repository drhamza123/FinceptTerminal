// MT5FleetPositionsTable.h — copied from HoldingsTable, adapted for broker positions
#pragma once
#include <QString>
#include <QVector>
#include <QWidget>
class QLabel; class QPushButton; class QTableWidget; class QTimer;
namespace fincept::screens {
struct PositionRow {
    QString symbol; QString side; double lots = 0; double entry = 0;
    double current = 0; double sl = 0; double tp = 0; double pnl = 0;
};
class MT5FleetPositionsTable : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetPositionsTable(QWidget* parent = nullptr);
    ~MT5FleetPositionsTable() override;
  signals:
    void position_selected(QString symbol);
  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
  private:
    void build_ui(); void apply_theme(); void refresh();
    QLabel* title_ = nullptr; QLabel* count_label_ = nullptr;
    QTableWidget* table_ = nullptr; QPushButton* close_btn_ = nullptr;
    QTimer* timer_ = nullptr;
};
} // namespace
