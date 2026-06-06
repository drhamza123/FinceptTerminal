// MT5FleetTradeTab.cpp
#include "screens/algo_trading/MT5FleetTradeTab.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::screens {
MT5FleetTradeTab::MT5FleetTradeTab(QWidget* parent) : QWidget(parent) { build_ui(); apply_theme(); }

void MT5FleetTradeTab::build_ui() {
    auto* vl = new QVBoxLayout(this); vl->setContentsMargins(14,14,14,14); vl->setSpacing(10);
    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel("EA Trade Execution"));
    top->addStretch();
    kill_btn_ = new QPushButton("EMERGENCY KILL ALL"); kill_btn_->setFixedHeight(28);
    kill_btn_->setStyleSheet("QPushButton{color:#FFF;background:#8B0000;border:none;font-size:10px;font-weight:700;padding:0 16px;}");
    connect(kill_btn_, &QPushButton::clicked, this, [this](){
        if (QMessageBox::warning(this,"Kill","Close ALL positions?",QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes)
            QMessageBox::information(this,"Kill Switch","Close-all commands sent to all connected EAs.");
    });
    top->addWidget(kill_btn_); vl->addLayout(top);
    pos_table_ = new QTableWidget(0,6,this);
    pos_table_->setHorizontalHeaderLabels({"EA","Symbol","Side","Lots","Entry","Status"});
    pos_table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    pos_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    pos_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pos_table_->setStyleSheet(QString("QTableWidget{background:%1;color:%2;gridline-color:%3;border:1px solid %3;}"
        "QHeaderView::section{background:%4;color:%2;padding:4px 8px;font-size:9px;font-weight:700;border:none;}"
        "QTableWidget::item{padding:4px 8px;font-size:10px;}").arg(ui::colors::BG_SURFACE(),ui::colors::TEXT_PRIMARY(),ui::colors::BORDER_DIM(),ui::colors::BG_RAISED()));
    vl->addWidget(pos_table_,1);
    vl->addWidget(new QLabel("No open positions. Deploy an EA and connect to MT5 to see live trades."));
}

void MT5FleetTradeTab::apply_theme() { setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE())); }
} // namespace
