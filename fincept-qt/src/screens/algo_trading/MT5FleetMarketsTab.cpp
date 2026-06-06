// MT5FleetMarketsTab.cpp — verbatim copy of MarketsTab for MT5 Fleet
#include "screens/algo_trading/MT5FleetMarketsTab.h"
#include "screens/crypto_center/panels/MarketsListPanel.h"
#include "ui/theme/Theme.h"
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

MT5FleetMarketsTab::MT5FleetMarketsTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fleetMarketsTab"); build_ui(); apply_theme();
}
MT5FleetMarketsTab::~MT5FleetMarketsTab() = default;

void MT5FleetMarketsTab::build_ui() {
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
    auto* scroll = new QScrollArea(this); scroll->setObjectName("fleetMarketsTabScroll");
    scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame); outer->addWidget(scroll);
    auto* content = new QWidget(scroll); content->setObjectName("fleetMarketsTabContent");
    auto* root = new QVBoxLayout(content); root->setContentsMargins(14,14,14,14); root->setSpacing(10);
    auto* host = new QFrame(content); host->setObjectName("fleetMarketsTabPanelHost");
    auto* hl = new QVBoxLayout(host); hl->setContentsMargins(0,0,0,0); hl->setSpacing(0);
    list_panel_ = new panels::MarketsListPanel(host); hl->addWidget(list_panel_);
    root->addWidget(host); root->addStretch(1); scroll->setWidget(content);
}

void MT5FleetMarketsTab::apply_theme() {
    setStyleSheet(QString(
        "QWidget#fleetMarketsTab{background:%1;}"
        "QScrollArea#fleetMarketsTabScroll{background:%1;border:none;}"
        "QWidget#fleetMarketsTabContent{background:%1;}"
        "QFrame#fleetMarketsTabPanelHost{background:%2;border:1px solid %3;}"
    ).arg(ui::colors::BG_BASE(),ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM()));
}

} // namespace fincept::screens
