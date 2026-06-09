// MT5FleetScreen.cpp — Fleet Center (copied from CryptoCenterScreen, adapted)
#include "screens/algo_trading/MT5FleetScreen.h"

#include "core/logging/Logger.h"
#include "screens/algo_trading/MT5FleetHoldingsBar.h"
#include "screens/algo_trading/MT5FleetHomeTab.h"
#include "screens/algo_trading/MT5FleetTradeTab.h"
#include "screens/algo_trading/MT5FleetActivityTab.h"
#include "screens/algo_trading/MT5FleetSettingsTab.h"
#include "screens/algo_trading/MT5FleetChartPanel.h"
#include "screens/algo_trading/MT5FleetOrderBookPanel.h"
#include "screens/algo_trading/MT5FleetSignalsPanel.h"
#include "screens/algo_trading/MT5FleetMarketplacePanel.h"
#include "screens/algo_trading/MT5FleetCloudPanel.h"
#include "screens/algo_trading/MT5FleetOrderPanel.h"
#include "screens/algo_trading/MT5FleetFreelancePanel.h"
#include "screens/algo_trading/MT5FleetVPSPanel.h"
#include "screens/algo_trading/MT5FleetMarketWatchPanel.h"
#include "screens/algo_trading/MT5FleetEcoCalendarPanel.h"
#include "screens/algo_trading/MT5FleetMultiChartContainer.h"
#include "screens/algo_trading/PriceAlertDialog.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

namespace fincept::screens {

static QString fleet_ff() { return "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace"; }

MT5FleetScreen::MT5FleetScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("mt5FleetScreen");
    build_ui();
    apply_theme();
    stack_->setCurrentWidget(empty_page_);
    check_connection();
}

MT5FleetScreen::~MT5FleetScreen() = default;

void MT5FleetScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    header_ = new QWidget(this);
    header_->setObjectName("fleetHeader");
    header_->setFixedHeight(40);
    auto* hl = new QHBoxLayout(header_);
    hl->setContentsMargins(14,0,14,0); hl->setSpacing(8);
    header_brand_ = new QLabel("GUARDIAN");
    header_brand_->setObjectName("fleetBrand");
    header_separator_ = new QLabel("/");
    header_separator_->setObjectName("fleetSep");
    header_route_ = new QLabel("MT5 FLEET");
    header_route_->setObjectName("fleetRoute");
    header_status_ = new QLabel("● DISCONNECTED");
    header_status_->setObjectName("fleetStatusOff");
    auto* alert_btn = new QPushButton("ALERTS", header_);
    alert_btn->setObjectName("fleetAlertBtn");
    alert_btn->setFixedHeight(22);
    alert_btn->setCursor(Qt::PointingHandCursor);
    connect(alert_btn, &QPushButton::clicked, this, [this]() {
        PriceAlertDialog dlg(this);
        dlg.exec();
    });
    hl->addWidget(header_brand_); hl->addWidget(header_separator_);
    hl->addWidget(header_route_); hl->addStretch(); hl->addWidget(alert_btn);
    hl->addSpacing(6); hl->addWidget(header_status_);
    root->addWidget(header_);
    stack_ = new QStackedWidget(this);
    stack_->setObjectName("fleetBody");
    root->addWidget(stack_, 1);
    build_empty_page();
    build_connected_page();
}

void MT5FleetScreen::build_empty_page() {
    empty_page_ = new QWidget(stack_);
    auto* layout = new QVBoxLayout(empty_page_);
    layout->setContentsMargins(14,14,14,14); layout->setSpacing(0);
    layout->addStretch(1);
    auto* center_row = new QHBoxLayout;
    center_row->addStretch(1);
    empty_panel_ = new QFrame(empty_page_);
    empty_panel_->setObjectName("fleetEmptyPanel");
    empty_panel_->setFixedWidth(520);
    auto* panel_l = new QVBoxLayout(empty_panel_);
    panel_l->setContentsMargins(0,0,0,0); panel_l->setSpacing(0);
    auto* head = new QWidget(empty_panel_);
    head->setObjectName("fleetPanelHead");
    head->setFixedHeight(34);
    auto* head_l = new QHBoxLayout(head);
    head_l->setContentsMargins(12,0,12,0); head_l->setSpacing(0);
    auto* head_title = new QLabel("CONNECT EA", head);
    head_title->setObjectName("fleetPanelTitle");
    head_l->addWidget(head_title); head_l->addStretch();
    auto* head_status = new QLabel("READY", head);
    head_status->setObjectName("fleetPanelStatus");
    head_l->addWidget(head_status);
    panel_l->addWidget(head);
    auto* body = new QWidget(empty_panel_);
    auto* body_l = new QVBoxLayout(body);
    body_l->setContentsMargins(20,18,20,20); body_l->setSpacing(10);
    empty_title_ = new QLabel(tr("No Expert Advisors connected"), body);
    empty_title_->setObjectName("fleetEmptyTitle");
    body_l->addWidget(empty_title_);
    empty_lede_ = new QLabel(
        tr("Deploy an EA to MetaTrader 5 to view your trading fleet, live "
           "balance, equity, and position status. The MT5 Bridge connects "
           "via TCP on localhost:5556."), body);
    empty_lede_->setObjectName("fleetEmptyLede");
    empty_lede_->setWordWrap(true);
    body_l->addWidget(empty_lede_);
    body_l->addSpacing(4);
    empty_security_label_ = new QLabel("SECURITY", body);
    empty_security_label_->setObjectName("fleetCaptionAccent");
    body_l->addWidget(empty_security_label_);
    empty_security_text_ = new QLabel(
        tr("· TCP bridge on 127.0.0.1:5556\n"
           "· line-delimited JSON protocol\n"
           "· EA identifies via magic number\n"
           "· read-only monitoring by default"), body);
    empty_security_text_->setObjectName("fleetSecurityText");
    body_l->addWidget(empty_security_text_);
    body_l->addSpacing(6);
    connect_button_ = new QPushButton(tr("CONNECT TO BRIDGE"), body);
    connect_button_->setObjectName("fleetPrimaryButton");
    connect_button_->setFixedHeight(28);
    connect_button_->setCursor(Qt::PointingHandCursor);
    connect(connect_button_, &QPushButton::clicked, this, [this]() {
        check_connection();
    });
    body_l->addWidget(connect_button_);
    panel_l->addWidget(body);
    center_row->addWidget(empty_panel_);
    center_row->addStretch(1);
    layout->addLayout(center_row);
    layout->addStretch(2);
    stack_->addWidget(empty_page_);
}

void MT5FleetScreen::build_connected_page() {
    connected_page_ = new QWidget(stack_);
    connected_page_->setObjectName("fleetConnectedPage");
    auto* root = new QVBoxLayout(connected_page_);
    root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    holdings_bar_ = new MT5FleetHoldingsBar(connected_page_);
    root->addWidget(holdings_bar_);
    tab_widget_ = new QTabWidget(connected_page_);
    tab_widget_->setObjectName("fleetTabs");
    tab_widget_->setDocumentMode(true);
    tab_widget_->setTabPosition(QTabWidget::North);
    tab_widget_->tabBar()->setExpanding(false);
    home_tab_ = new MT5FleetHomeTab(tab_widget_);
    trade_tab_ = new MT5FleetTradeTab(tab_widget_);
    activity_tab_ = new MT5FleetActivityTab(tab_widget_);
    settings_tab_ = new MT5FleetSettingsTab(tab_widget_);
    chart_panel_ = new MT5FleetChartPanel(tab_widget_);
    orderbook_panel_ = new MT5FleetOrderBookPanel(tab_widget_);
    signals_panel_ = new MT5FleetSignalsPanel(tab_widget_);
    marketplace_panel_ = new MT5FleetMarketplacePanel(tab_widget_);
    cloud_panel_ = new MT5FleetCloudPanel(tab_widget_);
    order_panel_ = new MT5FleetOrderPanel(tab_widget_);
    freelance_panel_ = new MT5FleetFreelancePanel(tab_widget_);
    vps_panel_ = new MT5FleetVPSPanel(tab_widget_);
    market_watch_panel_ = new MT5FleetMarketWatchPanel(tab_widget_);
    eco_calendar_panel_ = new MT5FleetEcoCalendarPanel(tab_widget_);
    multi_chart_container_ = new MT5FleetMultiChartContainer(tab_widget_);

    tab_widget_->addTab(home_tab_, tr("HOME"));
    tab_widget_->addTab(trade_tab_, tr("TRADE"));
    tab_widget_->addTab(activity_tab_, tr("ACTIVITY"));
    tab_widget_->addTab(settings_tab_, tr("SETTINGS"));
    tab_widget_->addTab(chart_panel_, tr("CHART"));
    tab_widget_->addTab(market_watch_panel_, tr("MARKETS"));
    tab_widget_->addTab(multi_chart_container_, tr("PRO CHARTS"));
    tab_widget_->addTab(orderbook_panel_, tr("ORDER BOOK"));
    tab_widget_->addTab(signals_panel_, tr("SIGNALS"));
    tab_widget_->addTab(marketplace_panel_, tr("MARKETPLACE"));
    tab_widget_->addTab(cloud_panel_, tr("CLOUD"));
    tab_widget_->addTab(order_panel_, tr("ORDER"));
    tab_widget_->addTab(freelance_panel_, tr("FREELANCE"));
    tab_widget_->addTab(vps_panel_, tr("VPS"));
    tab_widget_->addTab(eco_calendar_panel_, tr("CALENDAR"));
    root->addWidget(tab_widget_, 1);
    stack_->addWidget(connected_page_);
}

void MT5FleetScreen::apply_theme() {
    const QString font = fleet_ff();
    setStyleSheet(QString(
        "QWidget#mt5FleetScreen{background:%1;}"
        "QStackedWidget#fleetBody{background:%1;}"
        "QWidget#fleetConnectedPage{background:%1;}"
        "QWidget#fleetHeader{background:%2;border-bottom:1px solid %3;}"
        "QLabel#fleetBrand{color:%4;font-family:%5;font-size:11px;font-weight:700;letter-spacing:1.5px;background:transparent;}"
        "QLabel#fleetSep{color:%6;font-family:%5;font-size:11px;background:transparent;}"
        "QLabel#fleetRoute{color:%7;font-family:%5;font-size:11px;font-weight:600;letter-spacing:1.2px;background:transparent;}"
        "QLabel#fleetStatusOff{color:%8;font-family:%5;font-size:10px;font-weight:700;letter-spacing:1.2px;background:transparent;}"
        "QLabel#fleetStatusOn{color:%9;font-family:%5;font-size:10px;font-weight:700;letter-spacing:1.2px;background:transparent;}"
        "QFrame#fleetEmptyPanel{background:%2;border:1px solid %3;}"
        "QWidget#fleetPanelHead{background:%10;border-bottom:1px solid %3;}"
        "QLabel#fleetPanelTitle{color:%4;font-family:%5;font-size:11px;font-weight:700;letter-spacing:1.2px;background:transparent;}"
        "QLabel#fleetPanelStatus{color:%8;font-family:%5;font-size:10px;font-weight:700;letter-spacing:1.2px;background:transparent;}"
        "QLabel#fleetEmptyTitle{color:%7;font-family:%5;font-size:14px;font-weight:600;background:transparent;}"
        "QLabel#fleetEmptyLede{color:%11;font-family:%5;font-size:12px;background:transparent;}"
        "QLabel#fleetCaptionAccent{color:%4;font-family:%5;font-size:10px;font-weight:700;letter-spacing:1.5px;background:transparent;}"
        "QLabel#fleetSecurityText{color:%11;font-family:%5;font-size:11px;background:transparent;}"
        "QPushButton#fleetAlertBtn{background:rgba(239,68,68,0.10);color:%9;border:1px solid rgba(239,68,68,0.3);font-family:%5;font-size:9px;font-weight:700;letter-spacing:1.2px;padding:0 10px;border-radius:3px;}"
        "QPushButton#fleetAlertBtn:hover{background:rgba(239,68,68,0.25);}"
        "QPushButton#fleetPrimaryButton{background:rgba(217,119,6,0.10);color:%4;border:1px solid %12;font-family:%5;font-size:12px;font-weight:700;letter-spacing:1.5px;padding:0 16px;}"
        "QPushButton#fleetPrimaryButton:hover{background:%4;color:%1;}"
        "QTabWidget#fleetTabs::pane{background:%1;border:none;border-top:1px solid %3;}"
        "QTabWidget#fleetTabs QTabBar::tab{background:%2;color:%8;font-family:%5;font-size:10px;font-weight:700;letter-spacing:1.4px;padding:8px 14px;border-right:1px solid %3;}"
        "QTabWidget#fleetTabs QTabBar::tab:hover{background:%10;color:%7;}"
        "QTabWidget#fleetTabs QTabBar::tab:selected{background:%1;color:%4;border-bottom:2px solid %4;}"
        "QTabWidget#fleetTabs QTabBar{background:%2;border-bottom:1px solid %3;}"
    ).arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(),
          ui::colors::AMBER(), font, ui::colors::BORDER_BRIGHT(),
          ui::colors::TEXT_PRIMARY(), ui::colors::TEXT_TERTIARY(),
          ui::colors::POSITIVE(), ui::colors::BG_RAISED(),
          ui::colors::TEXT_SECONDARY(), "#78350f"));
}

void MT5FleetScreen::check_connection() {
    HttpClient::instance().get("/mt5/ea/list",
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            if (!arr.isEmpty()) {
                header_status_->setText(QString("● %1 EA%2").arg(arr.size()).arg(arr.size()==1?"":"s"));
                header_status_->setObjectName("fleetStatusOn");
                header_status_->style()->unpolish(header_status_);
                header_status_->style()->polish(header_status_);
                stack_->setCurrentWidget(connected_page_);
            }
        }, this);
}

void MT5FleetScreen::refreshFleet() { check_connection(); }

} // namespace fincept::screens
