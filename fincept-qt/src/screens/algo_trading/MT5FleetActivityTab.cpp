// MT5FleetActivityTab.cpp
#include "screens/algo_trading/MT5FleetActivityTab.h"
#include "ui/theme/Theme.h"
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace fincept::screens {
MT5FleetActivityTab::MT5FleetActivityTab(QWidget* parent) : QWidget(parent) { build_ui(); }

void MT5FleetActivityTab::build_ui() {
    auto* vl = new QVBoxLayout(this); vl->setContentsMargins(14,14,14,14);
    vl->addWidget(new QLabel("EA Activity Log"));
    log_table_ = new QTableWidget(0,6,this);
    log_table_->setHorizontalHeaderLabels({"Time","EA","Symbol","Action","Lots","Profit"});
    log_table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    log_table_->setStyleSheet(QString("QTableWidget{background:%1;color:%2;gridline-color:%3;border:1px solid %3;}"
        "QHeaderView::section{background:%4;color:%2;padding:4px 8px;font-size:9px;font-weight:700;border:none;}")
        .arg(ui::colors::BG_SURFACE(),ui::colors::TEXT_PRIMARY(),ui::colors::BORDER_DIM(),ui::colors::BG_RAISED()));
    vl->addWidget(log_table_,1);
    vl->addWidget(new QLabel("No activity yet. Trade events appear here in real-time via WebSocket."));
}
} // namespace
