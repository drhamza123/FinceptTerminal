// MT5FleetHomeTab.cpp — EA Home (copied from HomeTab)
#include "screens/algo_trading/MT5FleetHomeTab.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::screens {
namespace { QString home_ff() { return "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace"; } }

MT5FleetHomeTab::MT5FleetHomeTab(QWidget* parent) : QWidget(parent) { setObjectName("fleetHomeTab"); build_ui(); apply_theme(); refresh(); }
MT5FleetHomeTab::~MT5FleetHomeTab() = default;

void MT5FleetHomeTab::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(14,14,14,14); root->setSpacing(10);
    auto* top = new QHBoxLayout; top->setSpacing(10);
    fleet_panel_ = new QFrame(this); fleet_panel_->setObjectName("fleetTabPanel");
    auto* outer = new QVBoxLayout(fleet_panel_); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
    auto* head = new QWidget(fleet_panel_); head->setObjectName("fleetTabPanelHead"); head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head); hl->setContentsMargins(12,0,12,0);
    auto* title = new QLabel("EA FLEET", head); title->setObjectName("fleetTabPanelTitle");
    auto* status = new QLabel("● ACTIVE", head); status->setObjectName("fleetTabPanelStatusOk");
    hl->addWidget(title); hl->addStretch(); hl->addWidget(status); outer->addWidget(head);
    auto* body = new QWidget(fleet_panel_);
    auto* bl = new QVBoxLayout(body); bl->setContentsMargins(0,0,0,0); bl->setSpacing(0);
    auto add_row = [bl,body](const QString& cap, QLabel*& val) {
        auto* r = new QWidget(body); r->setObjectName("fleetTabRow"); r->setFixedHeight(32);
        auto* rl = new QHBoxLayout(r); rl->setContentsMargins(12,0,12,0);
        auto* c = new QLabel(cap, r); c->setObjectName("fleetTabCaption");
        val = new QLabel("—", r); val->setObjectName("fleetTabRowValue");
        rl->addWidget(c); rl->addStretch(); rl->addWidget(val); bl->addWidget(r);
    };
    add_row("CONNECTED EAs", row_count_value_);
    add_row("TOTAL BALANCE", row_balance_value_);
    add_row("STATUS", row_status_value_);
    outer->addWidget(body);
    auto* foot = new QWidget(fleet_panel_); foot->setObjectName("fleetTabPanelFoot"); foot->setFixedHeight(40);
    auto* fl = new QHBoxLayout(foot); fl->setContentsMargins(10,6,10,6);
    fl->addStretch();
    refresh_button_ = new QPushButton(tr("REFRESH"), foot); refresh_button_->setObjectName("fleetTabButton"); refresh_button_->setFixedHeight(26); refresh_button_->setCursor(Qt::PointingHandCursor);
    connect(refresh_button_, &QPushButton::clicked, this, &MT5FleetHomeTab::refresh);
    fl->addWidget(refresh_button_); outer->addWidget(foot);
    top->addWidget(fleet_panel_, 1);
    root->addLayout(top);

    ea_table_ = new QTableWidget(0,7,this); ea_table_->setObjectName("fleetTabTable");
    ea_table_->setHorizontalHeaderLabels({"EA","Symbol","TF","Magic","Status","Balance","Equity"});
    ea_table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    ea_table_->setSelectionBehavior(QAbstractItemView::SelectRows); ea_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(ea_table_, 1);
}

void MT5FleetHomeTab::apply_theme() {
    setStyleSheet(QString(
        "QFrame#fleetTabPanel{background:%1;border:1px solid %2;}"
        "QWidget#fleetTabPanelHead{background:%3;border-bottom:1px solid %2;}"
        "QLabel#fleetTabPanelTitle{color:%4;font-family:%5;font-size:11px;font-weight:700;letter-spacing:1.2px;background:transparent;}"
        "QLabel#fleetTabPanelStatusOk{color:%6;font-family:%5;font-size:10px;font-weight:700;letter-spacing:1.2px;background:transparent;}"
        "QWidget#fleetTabRow{background:%1;}"
        "QLabel#fleetTabCaption{color:%7;font-family:%5;font-size:10px;font-weight:700;letter-spacing:1px;background:transparent;}"
        "QLabel#fleetTabRowValue{color:%8;font-family:%5;font-size:12px;background:transparent;}"
        "QWidget#fleetTabPanelFoot{background:%3;border-top:1px solid %2;}"
        "QPushButton#fleetTabButton{background:rgba(217,119,6,0.10);color:%4;border:1px solid %9;font-family:%5;font-size:10px;font-weight:700;padding:0 12px;}"
        "QPushButton#fleetTabButton:hover{background:%4;color:%1;}"
        "QTableWidget#fleetTabTable{background:%1;color:%8;gridline-color:%2;border:1px solid %2;}"
        "QHeaderView::section{background:%3;color:%8;padding:4px 8px;font-size:9px;font-weight:700;border:none;border-bottom:1px solid %2;}"
        "QTableWidget::item{padding:4px 8px;font-size:10px;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
          ui::colors::AMBER(), home_ff(), ui::colors::POSITIVE(), ui::colors::TEXT_TERTIARY(),
          ui::colors::TEXT_PRIMARY(), "#78350f"));
}

void MT5FleetHomeTab::refresh() {
    HttpClient::instance().get("/mt5/ea/list",
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            int run=0; double bal=0;
            for (auto v : arr) {
                auto o = v.toObject();
                if (o["status"].toString()=="running") run++;
                bal += o["balance"].toDouble();
            }
            row_count_value_->setText(QString::number(arr.size()));
            row_balance_value_->setText(QString("$%1").arg(bal,0,'f',0));
            row_status_value_->setText(QString("%1 running").arg(run));

            ea_table_->setRowCount(arr.size());
            for (int i=0;i<arr.size();i++) {
                auto o = arr[i].toObject();
                auto* ni = new QTableWidgetItem(o["ea_name"].toString());
                ea_table_->setItem(i,0,ni);
                ea_table_->setItem(i,1,new QTableWidgetItem(o["symbol"].toString()));
                ea_table_->setItem(i,2,new QTableWidgetItem(o["timeframe"].toString()));
                ea_table_->setItem(i,3,new QTableWidgetItem(QString::number(o["magic_number"].toInt())));
                auto* si = new QTableWidgetItem(o["status"].toString().toUpper());
                si->setForeground(o["status"].toString()=="running"?QColor(ui::colors::POSITIVE()):QColor(ui::colors::NEGATIVE()));
                ea_table_->setItem(i,4,si);
                ea_table_->setItem(i,5,new QTableWidgetItem(QString("$%1").arg(o["balance"].toDouble(),0,'f',2)));
                ea_table_->setItem(i,6,new QTableWidgetItem(QString("$%1").arg(o["equity"].toDouble(),0,'f',2)));
            }
        }, this);
}

void MT5FleetHomeTab::showEvent(QShowEvent* e) { QWidget::showEvent(e); refresh(); }
void MT5FleetHomeTab::hideEvent(QHideEvent* e) { QWidget::hideEvent(e); }
} // namespace
