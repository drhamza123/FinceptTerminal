// MT5FleetMarketsTab.h — verbatim copy of MarketsTab for MT5 Fleet
#pragma once
#include <QWidget>
namespace fincept::screens::panels { class MarketsListPanel; }
namespace fincept::screens {
class MT5FleetMarketsTab : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetMarketsTab(QWidget* parent = nullptr);
    ~MT5FleetMarketsTab() override;
  private:
    void build_ui(); void apply_theme();
    panels::MarketsListPanel* list_panel_ = nullptr;
};
} // namespace
