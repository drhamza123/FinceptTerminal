// MT5FleetTreasuryPanel.cpp — EA treasury (adapted from TreasuryPanel)
#include "screens/algo_trading/MT5FleetTreasuryPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens {
namespace { QString treasury_ff() { return "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace"; } }

MT5FleetTreasuryPanel::MT5FleetTreasuryPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("fleetTreasuryPanel"); build_ui(); apply_theme();
}
MT5FleetTreasuryPanel::~MT5FleetTreasuryPanel() = default;

void MT5FleetTreasuryPanel::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    auto* head = new QWidget(this); head->setObjectName("fleetTreasuryHead"); head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head); hl->setContentsMargins(12,0,12,0);
    auto* title = new QLabel("EA TREASURY", head); title->setObjectName("fleetTreasuryTitle");
    status_pill_ = new QLabel("LIVE", head); status_pill_->setObjectName("fleetTreasuryPill");
    hl->addWidget(title); hl->addStretch(); hl->addWidget(status_pill_); root->addWidget(head);

    auto* body = new QWidget(this); body->setObjectName("fleetTreasuryBody");
    auto* bl = new QVBoxLayout(body); bl->setContentsMargins(14,14,14,14); bl->setSpacing(10);
    auto add = [&](const QString& c, QLabel*& v) {
        auto* r = new QHBoxLayout; auto* k = new QLabel(c,body); k->setObjectName("fleetTreasuryCaption");
        v = new QLabel("—",body); v->setObjectName("fleetTreasuryValue");
        r->addWidget(k); r->addStretch(); r->addWidget(v); bl->addLayout(r);
    };
    add("TOTAL BALANCE", total_balance_);
    add("TOTAL EQUITY", total_equity_);
    add("TOTAL P&L", total_pnl_);
    add("RUNNING EAs", running_eas_);
    bl->addStretch();
    root->addWidget(body,1);
}

void MT5FleetTreasuryPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#fleetTreasuryPanel{background:%1;}"
        "QWidget#fleetTreasuryHead{background:%2;border-bottom:1px solid %3;}"
        "QLabel#fleetTreasuryTitle{color:%4;font-family:%5;font-size:11px;font-weight:700;letter-spacing:1.4px;background:transparent;}"
        "QLabel#fleetTreasuryPill{color:%6;background:%7;border:1px solid %3;font-family:%5;font-size:9px;font-weight:700;letter-spacing:1.2px;padding:2px 8px;}"
        "QWidget#fleetTreasuryBody{background:%1;}"
        "QLabel#fleetTreasuryCaption{color:%8;font-family:%5;font-size:9px;font-weight:700;letter-spacing:1.2px;background:transparent;}"
        "QLabel#fleetTreasuryValue{color:%6;font-family:%5;font-size:12px;background:transparent;}"
    ).arg(ui::colors::BG_BASE(),ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM(),
          ui::colors::AMBER(),treasury_ff(),ui::colors::TEXT_PRIMARY(),ui::colors::BG_RAISED(),ui::colors::TEXT_TERTIARY()));
}
} // namespace
