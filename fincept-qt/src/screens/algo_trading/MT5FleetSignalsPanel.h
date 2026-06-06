// MT5FleetSignalsPanel.h — Copy Trading / Trading Signals
#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

namespace fincept::screens {

class MT5FleetSignalsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetSignalsPanel(QWidget* parent = nullptr);
    ~MT5FleetSignalsPanel() override;

  private slots:
    void refresh_signals();
    void on_copy_clicked();
    void on_filter_changed(int idx);

  private:
    void build_ui();
    void apply_theme();
    void update_signals_table(const QJsonArray& signal_list);

    // UI
    QComboBox* filter_combo_ = nullptr;
    QTableWidget* signals_table_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QLabel* status_label_ = nullptr;

    // State
    QString selected_signal_id_;
};

} // namespace fincept::screens
