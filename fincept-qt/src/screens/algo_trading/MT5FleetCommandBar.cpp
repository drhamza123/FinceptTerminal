// MT5FleetCommandBar.cpp
#include "screens/algo_trading/MT5FleetCommandBar.h"
#include "ui/theme/Theme.h"

namespace fincept::screens {
using namespace ui::colors;

MT5FleetCommandBar::MT5FleetCommandBar(QWidget* parent) : QWidget(parent) { build_ui(); }

void MT5FleetCommandBar::build_ui() {
    setFixedHeight(40);
    setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(BG_BASE(), BORDER_DIM()));
    auto* hl = new QHBoxLayout(this); hl->setContentsMargins(12,0,12,0); hl->setSpacing(8);

    status_dot_ = new QLabel("●"); status_dot_->setStyleSheet("color:#FFC400;font-size:14px;");
    hl->addWidget(status_dot_);

    brand_label_ = new QLabel("MT5 FLEET");
    brand_label_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1.5px;").arg(TEXT_PRIMARY()));
    hl->addWidget(brand_label_);

    auto* sep = new QLabel("|"); sep->setStyleSheet(QString("color:%1;").arg(BORDER_DIM())); hl->addWidget(sep);

    view_combo_ = new QComboBox;
    view_combo_->addItems({"Fleet", "Crypto", "Forex", "Stocks", "Trade"});
    view_combo_->setStyleSheet(QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 8px;font-size:9px;}")
        .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED()));
    hl->addWidget(view_combo_);
    connect(view_combo_, &QComboBox::currentIndexChanged, this, &MT5FleetCommandBar::view_changed);

    hl->addStretch();
    refresh_btn_ = new QPushButton("↻"); refresh_btn_->setFixedSize(28,28);
    refresh_btn_->setStyleSheet("QPushButton{background:transparent;color:#00E5FF;border:1px solid #00E5FF;font-size:16px;}");
    connect(refresh_btn_, &QPushButton::clicked, this, &MT5FleetCommandBar::refresh_requested);
    hl->addWidget(refresh_btn_);

    kill_btn_ = new QPushButton("KILL ALL"); kill_btn_->setFixedHeight(24);
    kill_btn_->setStyleSheet("QPushButton{color:#FFF;background:#8B0000;border:none;font-size:8px;font-weight:700;padding:0 10px;}");
    connect(kill_btn_, &QPushButton::clicked, this, &MT5FleetCommandBar::kill_requested);
    hl->addWidget(kill_btn_);
}

void MT5FleetCommandBar::set_refreshing(bool r) { refresh_btn_->setEnabled(!r); }
void MT5FleetCommandBar::set_has_selection(bool) {}
void MT5FleetCommandBar::refresh_theme() {}
void MT5FleetCommandBar::retranslateUi() {}
} // namespace
