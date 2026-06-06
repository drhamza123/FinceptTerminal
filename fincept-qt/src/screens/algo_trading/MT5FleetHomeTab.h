// MT5FleetHomeTab.h — EA Home (copied from HomeTab)
#pragma once
#include <QString>
#include <QVariant>
#include <QWidget>
class QFrame; class QLabel; class QPushButton; class QTableWidget;
namespace fincept::screens {
class MT5FleetHomeTab : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetHomeTab(QWidget* parent = nullptr);
    ~MT5FleetHomeTab() override;
  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
  private:
    void build_ui(); void apply_theme(); void refresh();
    QFrame* fleet_panel_ = nullptr;
    QLabel* row_count_value_ = nullptr;
    QLabel* row_balance_value_ = nullptr;
    QLabel* row_status_value_ = nullptr;
    QPushButton* refresh_button_ = nullptr;
    QTableWidget* ea_table_ = nullptr;
};
} // namespace fincept::screens
