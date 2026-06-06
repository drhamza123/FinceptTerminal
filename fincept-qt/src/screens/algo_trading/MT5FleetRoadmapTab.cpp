// MT5FleetRoadmapTab.cpp — verbatim copy of RoadmapTab for MT5 Fleet
#include "screens/algo_trading/MT5FleetRoadmapTab.h"
#include "screens/crypto_center/panels/BuybackBurnPanel.h"
#include "screens/crypto_center/panels/SupplyChartPanel.h"
#include "screens/crypto_center/panels/TreasuryPanel.h"
#include "ui/theme/Theme.h"
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

MT5FleetRoadmapTab::MT5FleetRoadmapTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fleetRoadmapTab"); build_ui(); apply_theme();
}
MT5FleetRoadmapTab::~MT5FleetRoadmapTab() = default;

void MT5FleetRoadmapTab::build_ui() {
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
    auto* scroll = new QScrollArea(this); scroll->setObjectName("fleetRoadmapTabScroll");
    scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame); outer->addWidget(scroll);
    auto* content = new QWidget(scroll); content->setObjectName("fleetRoadmapTabContent");
    auto* root = new QVBoxLayout(content); root->setContentsMargins(14,14,14,14); root->setSpacing(10);
    auto wrap = [content,root](QWidget* inner) {
        auto* host = new QFrame(content); host->setObjectName("fleetRoadmapTabPanelHost");
        auto* hl = new QVBoxLayout(host); hl->setContentsMargins(0,0,0,0); hl->setSpacing(0);
        hl->addWidget(inner); root->addWidget(host);
    };
    buyback_burn_ = new panels::BuybackBurnPanel(content); wrap(buyback_burn_);
    supply_chart_ = new panels::SupplyChartPanel(content); wrap(supply_chart_);
    treasury_ = new panels::TreasuryPanel(content); wrap(treasury_);
    root->addStretch(1); scroll->setWidget(content);
}

void MT5FleetRoadmapTab::apply_theme() {
    setStyleSheet(QString(
        "QWidget#fleetRoadmapTab{background:%1;}"
        "QScrollArea#fleetRoadmapTabScroll{background:%1;border:none;}"
        "QWidget#fleetRoadmapTabContent{background:%1;}"
        "QFrame#fleetRoadmapTabPanelHost{background:%2;border:1px solid %3;}"
    ).arg(ui::colors::BG_BASE(),ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM()));
}

} // namespace fincept::screens
