#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QTimer>
#include <QWebSocket>
#include <QJsonObject>

namespace fincept::screens {

class MT5FleetChartPanel;

class ExecutionPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ExecutionPanel(MT5FleetChartPanel* chart, QWidget* parent = nullptr);
    ~ExecutionPanel() override;

  private slots:
    void place_order();
    void close_position(const QString& symbol);
    void cancel_order(const QString& orderId);
    void refresh();

  private:
    void build_ui();
    void fetch_account();
    void fetch_positions();
    void fetch_orders();
    void update_position_markers();

    MT5FleetChartPanel* chart_ = nullptr;
    QTimer* timer_ = nullptr;

    // Controls
    QComboBox* symbol_combo_ = nullptr;
    QComboBox* side_combo_ = nullptr;
    QDoubleSpinBox* qty_spin_ = nullptr;
    QComboBox* type_combo_ = nullptr;
    QDoubleSpinBox* price_spin_ = nullptr;
    QDoubleSpinBox* sl_spin_ = nullptr;
    QDoubleSpinBox* tp_spin_ = nullptr;
    QDoubleSpinBox* trail_spin_ = nullptr;
    QPushButton* buy_btn_ = nullptr;
    QPushButton* sell_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;

    // Account
    QLabel* balance_lbl_ = nullptr;
    QLabel* equity_lbl_ = nullptr;
    QLabel* bp_lbl_ = nullptr;

    // Positions
    QTableWidget* pos_table_ = nullptr;

    // Orders
    QTableWidget* ord_table_ = nullptr;
};

} // namespace fincept::screens
