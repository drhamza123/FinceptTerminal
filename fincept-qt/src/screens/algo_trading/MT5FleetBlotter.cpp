// MT5FleetBlotter.cpp
#include "screens/algo_trading/MT5FleetBlotter.h"
#include "ui/theme/Theme.h"
#include <QHeaderView>
#include <QVBoxLayout>

namespace fincept::screens {
using namespace ui::colors;

MT5FleetBlotter::MT5FleetBlotter(QWidget* parent) : QWidget(parent) { build_ui(); }

void MT5FleetBlotter::build_ui() {
    auto* vl = new QVBoxLayout(this); vl->setContentsMargins(0,0,0,0); vl->setSpacing(0);
    auto* hdr = new QWidget(this); hdr->setFixedHeight(28);
    hdr->setStyleSheet(QString("background:%1;border-bottom:1px solid %2;").arg(BG_RAISED(),BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(12,0,12,0);
    title_label_ = new QLabel("EA FLEET"); title_label_->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;").arg(TEXT_PRIMARY()));
    hl->addWidget(title_label_);
    count_label_ = new QLabel("0"); count_label_->setStyleSheet(QString("color:%1;background:%2;border:1px solid %3;font-size:9px;font-weight:700;padding:0 6px;").arg(TEXT_SECONDARY(),BG_RAISED(),BORDER_DIM()));
    hl->addWidget(count_label_); hl->addStretch();
    filter_edit_ = new QLineEdit; filter_edit_->setPlaceholderText("Filter EAs…");
    filter_edit_->setStyleSheet(QString("QLineEdit{background:%1;color:%2;border:1px solid %3;font-size:10px;padding:2px 8px;}").arg(BG_BASE(),TEXT_SECONDARY(),BORDER_DIM()));
    connect(filter_edit_,&QLineEdit::textChanged,this,&MT5FleetBlotter::set_filter);
    hl->addWidget(filter_edit_); vl->addWidget(hdr);

    table_ = new QTableWidget(0,7,this);
    table_->setHorizontalHeaderLabels({"EA","Symbol","TF","Magic","Status","Balance","Equity"});
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows); table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setStyleSheet(QString("QTableWidget{background:%1;color:%2;gridline-color:%3;border:none;}"
        "QHeaderView::section{background:%4;color:%5;padding:4px 8px;font-size:9px;font-weight:700;border:none;}"
        "QTableWidget::item{padding:4px 8px;font-size:10px;}").arg(BG_BASE(),TEXT_PRIMARY(),BORDER_DIM(),BG_RAISED(),TEXT_SECONDARY()));
    connect(table_,&QTableWidget::cellClicked,this,[this](int r,int){
        if(r<0||r>=instances_.size())return;
        emit instance_selected(instances_[r].eid, instances_[r].symbol);
    });
    vl->addWidget(table_,1);
    setMinimumHeight(200);
}

void MT5FleetBlotter::set_instances(const QVector<mt5::EAInstance>& instances) {
    instances_ = instances; apply_filters();
    count_label_->setText(QString::number(instances.size()));
}

void MT5FleetBlotter::set_filter(const QString& text) { filter_text_ = text; apply_filters(); }

void MT5FleetBlotter::apply_filters() {
    int show=0;
    for(int i=0;i<instances_.size();++i){
        auto& e=instances_[i];
        bool match=filter_text_.isEmpty()||e.ea_name.contains(filter_text_,Qt::CaseInsensitive)||e.symbol.contains(filter_text_,Qt::CaseInsensitive);
        if(i>=table_->rowCount()) table_->setRowCount(i+1);
        if(!match){table_->setRowHidden(i,true);continue;}
        table_->setRowHidden(i,false);
        auto* ni=new QTableWidgetItem(e.ea_name); ni->setData(Qt::UserRole,e.eid);
        table_->setItem(show,0,ni);
        table_->setItem(show,1,new QTableWidgetItem(e.symbol));
        table_->setItem(show,2,new QTableWidgetItem(e.timeframe));
        table_->setItem(show,3,new QTableWidgetItem(QString::number(e.magic_number)));
        auto* si=new QTableWidgetItem(e.status.toUpper());
        si->setForeground(e.status=="running"?QColor(POSITIVE()):QColor(NEGATIVE()));
        table_->setItem(show,4,si);
        table_->setItem(show,5,new QTableWidgetItem(QString("$%1").arg(e.balance,0,'f',2)));
        table_->setItem(show,6,new QTableWidgetItem(QString("$%1").arg(e.equity,0,'f',2)));
        ++show;
    }
    for(int i=show;i<table_->rowCount();++i) table_->setRowHidden(i,true);
}

void MT5FleetBlotter::refresh_theme() {}
void MT5FleetBlotter::retranslateUi() {}
} // namespace
