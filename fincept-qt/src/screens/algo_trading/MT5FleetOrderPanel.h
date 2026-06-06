// MT5FleetOrderPanel.h — REST-backed Advanced Order Panel
#pragma once
#include <QWidget>
class QComboBox; class QFrame; class QLabel; class QLineEdit; class QPushButton;
class QTableWidget; class QDoubleSpinBox; class QCheckBox;
namespace fincept::screens {
class MT5FleetOrderPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetOrderPanel(QWidget* parent = nullptr);
    ~MT5FleetOrderPanel() override;
    void set_symbol(const QString& sym);
    void set_balance(double bal);
  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
  private slots:
    void refresh_positions();
  private:
    void build_ui(); void apply_theme();
    void place_order(const QString& side);
    void on_amount_changed(const QString& s);
    void on_symbol_changed(int idx);
    void on_order_type_changed(int idx);
    void update_balance_label();
    void show_error(const QString& msg);
    void hide_error();

    QComboBox* symbol_combo_ = nullptr;
    QComboBox* order_type_combo_ = nullptr;
    QLineEdit* amount_input_ = nullptr;
    QPushButton* max_button_ = nullptr;
    QLabel* balance_label_ = nullptr;
    QLabel* estimate_label_ = nullptr;
    QLabel* order_status_label_ = nullptr;
    QFrame* error_strip_ = nullptr;
    QLabel* error_text_ = nullptr;
    QPushButton* buy_button_ = nullptr;
    QPushButton* sell_button_ = nullptr;

    QWidget* advanced_params_ = nullptr;
    QDoubleSpinBox* limit_price_spin_ = nullptr;
    QDoubleSpinBox* stop_price_spin_ = nullptr;
    QDoubleSpinBox* sl_spin_ = nullptr;
    QDoubleSpinBox* tp_spin_ = nullptr;
    QDoubleSpinBox* trailing_dist_spin_ = nullptr;
    QCheckBox* trailing_stop_cb_ = nullptr;

    QWidget* oco_params_ = nullptr;
    QDoubleSpinBox* oco_limit_spin_ = nullptr;
    QDoubleSpinBox* oco_stop_spin_ = nullptr;
    QComboBox* oto_trigger_combo_ = nullptr;
    QDoubleSpinBox* oto_trigger_price_ = nullptr;

    QTableWidget* positions_table_ = nullptr;

    QString current_symbol_;
    double current_balance_ = 0;
    bool busy_ = false;
};
} // namespace
