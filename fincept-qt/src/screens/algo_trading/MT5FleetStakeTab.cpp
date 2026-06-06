// MT5FleetStakeTab.cpp — verbatim copy of StakeTab for MT5 Fleet
#include "screens/algo_trading/MT5FleetStakeTab.h"
#include "screens/crypto_center/panels/ActiveLocksPanel.h"
#include "screens/crypto_center/panels/LockPanel.h"
#include "screens/crypto_center/panels/TierPanel.h"
#include "ui/theme/Theme.h"
#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

MT5FleetStakeTab::MT5FleetStakeTab(QWidget* parent) : QWidget(parent) {
    setObjectName("fleetStakeTab"); build_ui(); apply_theme();
}
MT5FleetStakeTab::~MT5FleetStakeTab() = default;

void MT5FleetStakeTab::build_ui() {
    auto* outer = new QVBoxLayout(this); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
    auto* scroll = new QScrollArea(this); scroll->setObjectName("fleetStakeTabScroll");
    scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame); outer->addWidget(scroll);
    auto* content = new QWidget(scroll); content->setObjectName("fleetStakeTabContent");
    auto* root = new QVBoxLayout(content); root->setContentsMargins(14,14,14,14); root->setSpacing(10);
    auto wrap = [content,root](QWidget* inner) {
        auto* host = new QFrame(content); host->setObjectName("fleetStakeTabPanelHost");
        auto* hl = new QVBoxLayout(host); hl->setContentsMargins(0,0,0,0); hl->setSpacing(0);
        hl->addWidget(inner); root->addWidget(host);
    };
    lock_panel_ = new panels::LockPanel(content); wrap(lock_panel_);
    active_locks_ = new panels::ActiveLocksPanel(content); wrap(active_locks_);
    tier_panel_ = new panels::TierPanel(content); wrap(tier_panel_);
    root->addStretch(1); scroll->setWidget(content);
}

void MT5FleetStakeTab::apply_theme() {
    setStyleSheet(QString(
        "QWidget#fleetStakeTab{background:%1;}"
        "QScrollArea#fleetStakeTabScroll{background:%1;border:none;}"
        "QWidget#fleetStakeTabContent{background:%1;}"
        "QFrame#fleetStakeTabPanelHost{background:%2;border:1px solid %3;}"
    ).arg(ui::colors::BG_BASE(),ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM()));
}

} // namespace fincept::screens
