// MT5FleetSettingsTab.h
#pragma once
#include <QWidget>
class QLabel; class QPushButton; class QLineEdit;
namespace fincept::screens {
class MT5FleetSettingsTab : public QWidget {
  public: explicit MT5FleetSettingsTab(QWidget* parent = nullptr);
  private: void build_ui();
  QLineEdit* host_edit_ = nullptr; QLineEdit* port_edit_ = nullptr;
  QPushButton* save_btn_ = nullptr; QLabel* status_label_ = nullptr;
};
} // namespace
