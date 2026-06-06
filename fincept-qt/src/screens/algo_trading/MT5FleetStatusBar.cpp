// MT5FleetStatusBar.cpp
#include "screens/algo_trading/MT5FleetStatusBar.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>
#include <QDateTime>

namespace fincept::screens {
using namespace ui::colors;

MT5FleetStatusBar::MT5FleetStatusBar(QWidget* parent) : QWidget(parent) { build_ui(); }

void MT5FleetStatusBar::build_ui() {
    setFixedHeight(22);
    setStyleSheet(QString("background:%1;border-top:1px solid %2;").arg(BG_RAISED(),BORDER_DIM()));
    auto* hl = new QHBoxLayout(this); hl->setContentsMargins(12,0,12,0);
    brand_ = new QLabel("MT5 BRIDGE"); brand_->setStyleSheet(QString("color:%1;font-size:8px;font-weight:700;").arg(CYAN()));
    hl->addWidget(brand_);
    engine_ = new QLabel("v1.0"); engine_->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    hl->addWidget(engine_);
    hl->addStretch();
    connected_ = new QLabel("0 EA"); connected_->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    hl->addWidget(connected_);
    pnl_ = new QLabel(""); pnl_->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    hl->addWidget(pnl_);
    time_ = new QLabel(); time_->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    hl->addWidget(time_);
    timer_ = new QTimer(this);
    connect(timer_,&QTimer::timeout,this,[this](){ time_->setText(QDateTime::currentDateTime().toString("HH:mm:ss")); });
}

void MT5FleetStatusBar::set_summary(const mt5::EASummary& s) {
    connected_->setText(QString("%1 EA").arg(s.running_count));
    pnl_->setText(QString("P&L: $%1").arg(s.total_pnl,0,'f',2));
    pnl_->setStyleSheet(QString("color:%1;font-size:8px;").arg(s.total_pnl>=0?POSITIVE():NEGATIVE()));
}

void MT5FleetStatusBar::start_clock() { timer_->start(1000); time_->setText(QDateTime::currentDateTime().toString("HH:mm:ss")); }
} // namespace
