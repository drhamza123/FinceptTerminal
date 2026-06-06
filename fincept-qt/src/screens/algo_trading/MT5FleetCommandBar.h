// src/screens/algo_trading/MT5FleetCommandBar.h
#pragma once
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace fincept::screens {

class MT5FleetCommandBar : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetCommandBar(QWidget* parent = nullptr);
    void set_refreshing(bool refreshing);
    void set_has_selection(bool has);
    void refresh_theme();

  signals:
    void refresh_requested();
    void kill_requested();
    void view_changed(int index);

  private:
    void build_ui();
    void retranslateUi();

    QLabel* brand_label_ = nullptr;
    QLabel* status_dot_ = nullptr;
    QComboBox* view_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* kill_btn_ = nullptr;
};

} // namespace fincept::screens
