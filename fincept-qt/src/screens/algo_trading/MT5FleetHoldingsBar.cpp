// MT5FleetHoldingsBar.cpp — EA fleet bar (copied from HoldingsBar)
#include "screens/algo_trading/MT5FleetHoldingsBar.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QDateTime>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLocale>
#include <QShowEvent>
#include <QStyle>

namespace fincept::screens {

static QString hbar_ff() { return "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace"; }

MT5FleetHoldingsBar::MT5FleetHoldingsBar(QWidget* parent) : QWidget(parent) {
    setObjectName("fleetHoldingsBar");
    setFixedHeight(56);
    build_ui();
    apply_theme();
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(5000);
    connect(refresh_timer_, &QTimer::timeout, this, &MT5FleetHoldingsBar::refresh);
    refresh();
}

MT5FleetHoldingsBar::~MT5FleetHoldingsBar() = default;

void MT5FleetHoldingsBar::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(14,8,14,8); root->setSpacing(18);
    auto add = [this,root](const QString& label, QLabel*& val, const QString& on) {
        auto* col = new QVBoxLayout;
        col->setContentsMargins(0,0,0,0); col->setSpacing(2);
        auto* l = new QLabel(label, this); l->setObjectName("fleetHBarLabel");
        val = new QLabel("—", this); val->setObjectName(on);
        col->addWidget(l); col->addWidget(val); root->addLayout(col);
    };
    add("EAs", ea_count_, "fleetHBarValue");
    add("BALANCE", total_balance_, "fleetHBarValue");
    add("P&L", total_pnl_, "fleetHBarValue");
    add("WIN RATE", win_rate_, "fleetHBarValue");
    add("RUNNING", running_, "fleetHBarValueAccent");
    add("UPDATED", updated_value_, "fleetHBarValueDim");
    root->addStretch(1);
    feed_status_ = new QLabel("● LIVE", this);
    feed_status_->setObjectName("fleetHBarFeedLive");
    root->addWidget(feed_status_);
}

void MT5FleetHoldingsBar::apply_theme() {
    setStyleSheet(QString(
        "QWidget#fleetHoldingsBar{background:%1;border-bottom:1px solid %2;}"
        "QLabel#fleetHBarLabel{color:%3;font-family:%4;font-size:9px;font-weight:700;letter-spacing:1.5px;background:transparent;}"
        "QLabel#fleetHBarValue{color:%5;font-family:%4;font-size:14px;font-weight:600;background:transparent;}"
        "QLabel#fleetHBarValueAccent{color:%6;font-family:%4;font-size:14px;font-weight:700;background:transparent;}"
        "QLabel#fleetHBarValueDim{color:%7;font-family:%4;font-size:12px;background:transparent;}"
        "QLabel#fleetHBarFeedLive{color:%8;font-family:%4;font-size:10px;font-weight:700;letter-spacing:1.2px;background:transparent;padding:2px 6px;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_TERTIARY(),
          hbar_ff(), ui::colors::TEXT_PRIMARY(), ui::colors::AMBER(),
          ui::colors::TEXT_SECONDARY(), ui::colors::POSITIVE()));
}

void MT5FleetHoldingsBar::refresh() {
    HttpClient::instance().get("http://localhost:8150/mt5/ea/list",
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            int total=arr.size(), run=0; double bal=0, pnl=0;
            for (auto v : arr) {
                auto o = v.toObject();
                if (o["status"].toString()=="running") run++;
                bal += o["balance"].toDouble();
                pnl += o["pnl"].toDouble();
            }
            ea_count_->setText(QString::number(total));
            total_balance_->setText(QString("$%1").arg(bal,0,'f',0));
            total_pnl_->setText(QString("$%1").arg(pnl,0,'f',2));
            total_pnl_->setStyleSheet(QString("color:%1;font-family:%2;font-size:14px;font-weight:600;background:transparent;")
                .arg(pnl>=0?ui::colors::POSITIVE():ui::colors::NEGATIVE(), hbar_ff()));
            win_rate_->setText("--");
            running_->setText(QString::number(run));
            updated_value_->setText("just now");
        }, this);
}

void MT5FleetHoldingsBar::showEvent(QShowEvent* e) { QWidget::showEvent(e); if (!refresh_timer_->isActive()) refresh_timer_->start(); refresh(); }
void MT5FleetHoldingsBar::hideEvent(QHideEvent* e) { QWidget::hideEvent(e); refresh_timer_->stop(); }

} // namespace fincept::screens
