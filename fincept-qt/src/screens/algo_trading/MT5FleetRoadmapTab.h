// MT5FleetRoadmapTab.h — verbatim copy of RoadmapTab for MT5 Fleet
#pragma once
#include <QWidget>
namespace fincept::screens::panels { class BuybackBurnPanel; class SupplyChartPanel; class TreasuryPanel; }
namespace fincept::screens {
class MT5FleetRoadmapTab : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetRoadmapTab(QWidget* parent = nullptr);
    ~MT5FleetRoadmapTab() override;
  private:
    void build_ui(); void apply_theme();
    panels::BuybackBurnPanel* buyback_burn_ = nullptr;
    panels::SupplyChartPanel* supply_chart_ = nullptr;
    panels::TreasuryPanel* treasury_ = nullptr;
};
} // namespace
