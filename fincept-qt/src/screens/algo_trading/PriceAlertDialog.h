// PriceAlertDialog.h — Price Alert management dialog
#pragma once
#include <QDialog>
#include <QVector>

class QTableWidget;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QCheckBox;

namespace fincept::screens {

struct PriceAlert {
    QString id;
    QString symbol;
    QString alert_type; // "above", "below", "cross_above", "cross_below", "range"
    double trigger_price = 0;
    double range_high = 0;
    bool enabled = true;
    QString channel; // "telegram", "email", "push", "sound"
    QString note;
    int triggered_count = 0;
    bool is_triggered = false;
};

class PriceAlertDialog : public QDialog {
    Q_OBJECT
public:
    explicit PriceAlertDialog(QWidget* parent = nullptr);
    ~PriceAlertDialog() override;

signals:
    void alertsChanged();

private slots:
    void on_add_alert();
    void on_delete_alert();
    void on_toggle_alert();
    void on_alert_type_changed(int idx);

private:
    void build_ui();
    void apply_theme();
    void refresh_alerts();
    void load_alerts();
    void save_alert(const PriceAlert& alert);
    void delete_alert(const QString& id);

    // Alert form
    QComboBox* symbol_combo_ = nullptr;
    QComboBox* alert_type_combo_ = nullptr;
    QDoubleSpinBox* trigger_price_spin_ = nullptr;
    QLabel* range_label_ = nullptr;
    QDoubleSpinBox* range_high_spin_ = nullptr;
    QComboBox* channel_combo_ = nullptr;
    QLineEdit* note_input_ = nullptr;
    QPushButton* add_btn_ = nullptr;

    // Alert list
    QTableWidget* alert_table_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QPushButton* toggle_btn_ = nullptr;
    QLabel* status_label_ = nullptr;

    QVector<PriceAlert> alerts_;
    QStringList all_symbols_;
};

} // namespace fincept::screens
