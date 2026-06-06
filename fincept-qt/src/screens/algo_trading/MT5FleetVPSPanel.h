// MT5FleetVPSPanel.h — VPS Management for MT5 EA Hosting
#pragma once
#include <QWidget>
class QTableWidget; class QPushButton; class QLabel; class QComboBox;

namespace fincept::screens {

class MT5FleetVPSPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetVPSPanel(QWidget* parent = nullptr);
    ~MT5FleetVPSPanel() override;

  private slots:
    void refresh_plans();
    void refresh_instances();
    void on_deploy();
    void on_action(const QString& action);

  private:
    void build_ui();
    void apply_theme();

    QTableWidget* plans_table_ = nullptr;
    QTableWidget* instances_table_ = nullptr;
    QPushButton* deploy_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QComboBox* plan_combo_ = nullptr;
    QLabel* instance_status_label_ = nullptr;
    QLabel* status_label_ = nullptr;
};

} // namespace fincept::screens
