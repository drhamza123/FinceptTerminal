// MT5FleetPositionsTable.cpp — copied from HoldingsTable, adapted for EA positions
#include "screens/algo_trading/MT5FleetPositionsTable.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {
static QString posFf() { return "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace"; }
static QString posFmt(double v, int d=2) { return QLocale::system().toString(v,'f',d); }
}

MT5FleetPositionsTable::MT5FleetPositionsTable(QWidget* parent) : QWidget(parent) {
    setObjectName("fleetPositionsTable"); build_ui(); apply_theme();
    timer_ = new QTimer(this); timer_->setInterval(5000);
    connect(timer_,&QTimer::timeout,this,&MT5FleetPositionsTable::refresh);
}
MT5FleetPositionsTable::~MT5FleetPositionsTable() = default;

void MT5FleetPositionsTable::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    auto* head = new QWidget(this); head->setObjectName("fleetPosHead"); head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head); hl->setContentsMargins(12,0,12,0);
    title_ = new QLabel("OPEN POSITIONS", head); title_->setObjectName("fleetPosTitle");
    count_label_ = new QLabel("0", head); count_label_->setObjectName("fleetPosCount");
    close_btn_ = new QPushButton("CLOSE ALL", head); close_btn_->setObjectName("fleetPosCloseBtn");
    close_btn_->setFixedHeight(24); close_btn_->setCursor(Qt::PointingHandCursor);
    hl->addWidget(title_); hl->addWidget(count_label_); hl->addStretch(); hl->addWidget(close_btn_);
    root->addWidget(head);

    table_ = new QTableWidget(0,9,this); table_->setObjectName("fleetPosTable");
    table_->setHorizontalHeaderLabels({"EA","Symbol","Side","Lots","Entry","Current","SL","TP","P&L"});
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setDefaultSectionSize(28);
    root->addWidget(table_,1);
}

void MT5FleetPositionsTable::apply_theme() {
    setStyleSheet(QString(
        "QWidget#fleetPositionsTable{background:%1;}"
        "QWidget#fleetPosHead{background:%2;border-bottom:1px solid %3;}"
        "QLabel#fleetPosTitle{color:%4;font-family:%5;font-size:11px;font-weight:700;letter-spacing:1.4px;background:transparent;}"
        "QLabel#fleetPosCount{color:%6;background:%7;border:1px solid %3;font-family:%5;font-size:9px;font-weight:700;padding:2px 8px;}"
        "QPushButton#fleetPosCloseBtn{color:#FFF;background:#8B0000;border:none;font-family:%5;font-size:9px;font-weight:700;padding:0 10px;}"
        "QTableWidget#fleetPosTable{background:%1;color:%6;gridline-color:%3;border:none;}"
        "QHeaderView::section{background:%2;color:%6;padding:4px 8px;font-size:9px;font-weight:700;border:none;border-bottom:1px solid %3;}"
        "QTableWidget::item{padding:2px 8px;font-size:10px;}"
    ).arg(ui::colors::BG_BASE(),ui::colors::BG_SURFACE(),ui::colors::BORDER_DIM(),
          ui::colors::AMBER(),posFf(),ui::colors::TEXT_PRIMARY(),ui::colors::BG_RAISED()));
}

void MT5FleetPositionsTable::refresh() {
    HttpClient::instance().get("http://localhost:8150/mt5/ea/list",
        [this](Result<QJsonDocument> r) {
            if(r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            table_->setRowCount(0);
            for(auto v : arr) {
                auto o = v.toObject();
                if(o["status"].toString()!="running") continue;
                int row = table_->rowCount(); table_->setRowCount(row+1);
                table_->setItem(row,0,new QTableWidgetItem(o["ea_name"].toString()));
                table_->setItem(row,1,new QTableWidgetItem(o["symbol"].toString()));
                table_->setItem(row,2,new QTableWidgetItem("—"));
                table_->setItem(row,3,new QTableWidgetItem("—"));
                table_->setItem(row,4,new QTableWidgetItem(QString("$%1").arg(o["balance"].toDouble(),0,'f',2)));
                table_->setItem(row,5,new QTableWidgetItem(QString("$%1").arg(o["equity"].toDouble(),0,'f',2)));
                table_->setItem(row,6,new QTableWidgetItem("—"));
                table_->setItem(row,7,new QTableWidgetItem("—"));
                auto* pi = new QTableWidgetItem(QString("$%1").arg(o["pnl"].toDouble(),0,'f',2));
                pi->setForeground(o["pnl"].toDouble()>=0?QColor(ui::colors::POSITIVE()):QColor(ui::colors::NEGATIVE()));
                table_->setItem(row,8,pi);
            }
            count_label_->setText(QString::number(table_->rowCount()));
        },this);
}

void MT5FleetPositionsTable::showEvent(QShowEvent* e) { QWidget::showEvent(e); timer_->start(); refresh(); }
void MT5FleetPositionsTable::hideEvent(QHideEvent* e) { QWidget::hideEvent(e); timer_->stop(); }
} // namespace
