// MT5FleetCloudPanel.h — Cloud Strategy Optimization
#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>

namespace fincept::screens {

class MT5FleetCloudPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetCloudPanel(QWidget* parent = nullptr);
    ~MT5FleetCloudPanel() override;

  private slots:
    void refresh_optimizations();
    void on_start_clicked();
    void on_stop_clicked();

  private:
    void build_ui();
    void apply_theme();
    void update_optimizations_table(const QJsonArray& optimizations);

    // UI - Configuration
    QComboBox* ea_combo_ = nullptr;
    QComboBox* symbol_combo_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QSpinBox* agents_spin_ = nullptr;
    QSpinBox* generations_spin_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;

    // UI - Status
    QProgressBar* progress_bar_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* agents_label_ = nullptr;

    // UI - Results
    QTableWidget* results_table_ = nullptr;

    // State
    QString current_optimization_id_;
};

} // namespace fincept::screens
