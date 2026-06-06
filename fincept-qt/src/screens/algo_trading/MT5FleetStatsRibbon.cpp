// MT5FleetStatsRibbon.cpp
#include "screens/algo_trading/MT5FleetStatsRibbon.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>

namespace fincept::screens {
using namespace ui::colors;

MT5FleetStatsRibbon::MT5FleetStatsRibbon(QWidget* parent) : QWidget(parent) { build_ui(); }

QWidget* MT5FleetStatsRibbon::build_hero(HeroCell& cell, const QString& label_text, int vpx) {
    auto* w = new QWidget(this); auto* vl = new QVBoxLayout(w); vl->setContentsMargins(0,0,0,0); vl->setSpacing(1);
    cell.label = new QLabel(label_text); cell.label->setStyleSheet(QString("color:%1;font-size:7px;letter-spacing:0.8px;").arg(TEXT_TERTIARY()));
    cell.value = new QLabel("--"); cell.value->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;").arg(TEXT_PRIMARY()).arg(vpx));
    cell.sub = new QLabel(""); cell.sub->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    vl->addWidget(cell.label); vl->addWidget(cell.value); vl->addWidget(cell.sub); vl->addStretch();
    return w;
}

void MT5FleetStatsRibbon::build_ui() {
    setFixedHeight(70);
    setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(BG_RAISED(), BORDER_DIM()));
    auto* hl = new QHBoxLayout(this); hl->setContentsMargins(12,6,12,4); hl->setSpacing(4);

    auto addSep = [&]() { auto* s = new QWidget(this); s->setFixedSize(1,40); s->setStyleSheet(QString("background:%1;").arg(BORDER_DIM())); hl->addWidget(s); };
    auto makeCell = [&](HeroCell& c, const QString& lbl, int sz, const QString& color) {
        hl->addWidget(build_hero(c, lbl, sz)); c.value->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;").arg(color).arg(sz));
        addSep();
    };
    makeCell(connected_, "CONNECTED EAs", 16, CYAN()); makeCell(pnl_, "TOTAL P&L", 16, POSITIVE());
    makeCell(winrate_, "WIN RATE", 14, AMBER()); makeCell(trades_, "TOTAL TRADES", 14, TEXT_TERTIARY);
    makeCell(balance_, "BALANCE", 16, TEXT_PRIMARY); makeCell(today_, "TODAY", 14, POSITIVE());
    hl->addStretch();
}

void MT5FleetStatsRibbon::set_summary(const mt5::EASummary& s) {
    connected_.value->setText(QString::number(s.running_count));
    pnl_.value->setText(QString("$%1").arg(s.total_pnl,0,'f',2));
    pnl_.value->setStyleSheet(QString("color:%1;font-size:16px;font-weight:700;").arg(s.total_pnl>=0?POSITIVE():NEGATIVE()));
    winrate_.value->setText(QString("%1%").arg(s.win_rate*100,0,'f',1));
    trades_.value->setText(QString::number(s.total_trades));
    balance_.value->setText(QString("$%1").arg(s.total_balance,0,'f',0));
}
} // namespace
