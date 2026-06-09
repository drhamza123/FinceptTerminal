// MT5FleetPanel.cpp — Portfolio-style EA Fleet + Markets
#include "screens/algo_trading/MT5FleetPanel.h"
#include "core/config/AppConfig.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QUrl>

namespace fincept::screens {

using namespace fincept::ui::colors;

namespace {
QString mt5_ws_url(const QString& path) {
    QUrl url(fincept::AppConfig::instance().api_base_url());
    const QString scheme = url.scheme().toLower() == QStringLiteral("https") ? QStringLiteral("wss")
                                                                             : QStringLiteral("ws");
    url.setScheme(scheme);
    url.setPath(path);
    return url.toString();
}
} // namespace

static const QStringList kCryptoS = {"BTCUSD","ETHUSD","SOLUSD","XRPUSD","ADAUSD","DOTUSD"};
static const QStringList kForexS = {"EURUSD","GBPUSD","USDJPY","AUDUSD","USDCAD","NZDUSD"};
static const QStringList kStockS = {"AAPL","MSFT","GOOGL","AMZN","TSLA","META","NVDA"};

MT5FleetPanel::MT5FleetPanel(QWidget* parent)
    : QWidget(parent), chart_(nullptr), chart_view_(nullptr)
    , ws_(new QWebSocket("", QWebSocketProtocol::VersionLatest, this))
    , reconnect_timer_(new QTimer(this)), api_base_(""), ws_connected_(false)
{
    setupUi();
    connect(ws_, &QWebSocket::connected, this, [this](){
        ws_connected_=true; reconnect_timer_->stop();
        label_ws_->setText("●"); label_ws_->setStyleSheet("color:#22c55e;font-size:16px;");
        refreshEAs();
    });
    connect(ws_, &QWebSocket::textMessageReceived, this, [this](const QString& msg){
        auto o=QJsonDocument::fromJson(msg.toUtf8()).object();
        QString t=o["type"].toString();
        if(t=="ea_status"||t=="ea_heartbeat"||t=="ea_trade") refreshEAs();
    });
    connect(ws_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, [this](){
        ws_connected_=false; label_ws_->setText("✕"); label_ws_->setStyleSheet("color:#ef4444;font-size:16px;");
        reconnect_timer_->start();
    });
    reconnect_timer_->setInterval(10000); reconnect_timer_->setSingleShot(true);

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(10000);
    connect(refresh_timer_, &QTimer::timeout, this, &MT5FleetPanel::refreshFleet);

    market_timer_ = new QTimer(this);
    market_timer_->setInterval(15000);
    connect(market_timer_, &QTimer::timeout, this, [this](){ refreshFleet(); });

    connect(market_tabs_, &QTabWidget::currentChanged, this, &MT5FleetPanel::onTabChanged);
    connect(view_selector_, &QComboBox::currentIndexChanged, this, &MT5FleetPanel::onTabChanged);

    ws_->open(QUrl(mt5_ws_url(QStringLiteral("/ws/mt5"))));
    refresh_timer_->start();
    refreshEAs();
}

void MT5FleetPanel::setupUi() {
    setStyleSheet(QString("background:%1;").arg(BG_BASE()));
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    // ── Command Bar (Portfolio-style) ──
    cmd_bar_ = new QWidget; cmd_bar_->setFixedHeight(36);
    cmd_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(BG_BASE(), BORDER_DIM()));
    auto* cbl = new QHBoxLayout(cmd_bar_); cbl->setContentsMargins(12,0,12,0);

    label_ws_ = new QLabel("·"); label_ws_->setStyleSheet("color:#FFC400;font-size:16px;");
    cbl->addWidget(label_ws_);

    label_title_ = new QLabel("MT5 FLEET");
    label_title_->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;").arg(TEXT_PRIMARY()));
    cbl->addWidget(label_title_);
    cbl->addWidget(new QLabel("|"));

    view_selector_ = new QComboBox;
    view_selector_->addItems({"Fleet View", "Crypto", "Forex", "Stocks", "Quick Trade"});
    view_selector_->setStyleSheet(QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:2px 8px;font-size:9px;}")
        .arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED()));
    cbl->addWidget(view_selector_);
    cbl->addStretch();

    stat_balance_ = new QLabel("Balance: --");
    stat_balance_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(TEXT_TERTIARY()));
    cbl->addWidget(stat_balance_);
    stat_connected_ = new QLabel("0 EA");
    stat_connected_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(CYAN_HEX));
    cbl->addWidget(stat_connected_);
    stat_pnl_ = new QLabel("P&L: --");
    cbl->addWidget(stat_pnl_);

    btn_refresh_ = new QPushButton("↻");
    btn_refresh_->setFixedSize(28,28);
    btn_refresh_->setStyleSheet("QPushButton{background:transparent;color:#00E5FF;border:1px solid #00E5FF;border-radius:0;font-size:16px;}");
    connect(btn_refresh_,&QPushButton::clicked,this,&MT5FleetPanel::refreshFleet);
    cbl->addWidget(btn_refresh_);

    btn_kill_all_ = new QPushButton("EMERGENCY KILL");
    btn_kill_all_->setFixedHeight(24);
    btn_kill_all_->setStyleSheet("QPushButton{color:#FFF;background:#8B0000;border:none;font-size:8px;font-weight:700;padding:0 10px;}");
    connect(btn_kill_all_,&QPushButton::clicked,this,[this](){
        if(QMessageBox::warning(this,"Kill Switch","Close ALL EA positions?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes) return;
        for(int r=0;r<ea_table_->rowCount();++r){
            auto* it=ea_table_->item(r,0); if(!it) continue;
            QString eid=it->data(Qt::UserRole).toString(); if(eid.isEmpty()) continue;
            HttpClient::instance().post(api_base_+"/mt5/ea/"+eid+"/close-all",QJsonObject{},[](Result<QJsonDocument>){},this);
        }
    });
    cbl->addWidget(btn_kill_all_);
    root->addWidget(cmd_bar_);

    // ── Stats Ribbon (Portfolio-style) ──
    stats_bar_ = new QWidget; stats_bar_->setFixedHeight(28);
    stats_bar_->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(BG_RAISED(), BORDER_DIM()));
    auto* sbl = new QHBoxLayout(stats_bar_); sbl->setContentsMargins(12,0,12,0); sbl->setSpacing(16);

    auto mkStat = [&](const QString& label, QLabel*& val, const QString& color) {
        auto* w = new QWidget; auto* hl = new QHBoxLayout(w); hl->setContentsMargins(0,0,0,0); hl->setSpacing(4);
        auto* lbl = new QLabel(label); lbl->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
        hl->addWidget(lbl);
        val = new QLabel("--"); val->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(color));
        hl->addWidget(val); sbl->addWidget(w);
    };
    mkStat("CONNECTED", stat_connected_, CYAN_HEX);
    mkStat("TOTAL P&L", stat_pnl_, "#22c55e");
    mkStat("WIN RATE", stat_win_rate_, AMBER_HEX);
    mkStat("TRADES", stat_total_trades_, TEXT_TERTIARY);
    mkStat("BALANCE", stat_balance_, TEXT_PRIMARY);
    mkStat("TODAY", stat_today_pnl_, "#22c55e");
    sbl->addStretch();
    root->addWidget(stats_bar_);

    // ── Content Stack ──
    content_stack_ = new QStackedWidget(this);

    // Page 0: EA Fleet View
    ea_view_ = new QWidget; auto* evl = new QVBoxLayout(ea_view_); evl->setContentsMargins(0,0,0,0);
    auto* splitter = new QSplitter(Qt::Vertical, ea_view_);
    ea_table_ = new QTableWidget(0,8,ea_view_);
    ea_table_->setHorizontalHeaderLabels({"EA Name","Symbol","TF","Magic","Status","Balance","Equity","Actions"});
    ea_table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    ea_table_->setSelectionBehavior(QAbstractItemView::SelectRows); ea_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ea_table_->setStyleSheet(QString("QTableWidget{background:%1;color:%2;gridline-color:%3;border:none;}"
        "QHeaderView::section{background:%4;color:%5;padding:4px 8px;font-size:9px;font-weight:700;border:none;}"
        "QTableWidget::item{padding:4px 8px;font-size:10px;}").arg(BG_BASE(),TEXT_PRIMARY(),BORDER_DIM(),BG_RAISED(),TEXT_SECONDARY()));
    connect(ea_table_,&QTableWidget::cellClicked,this,&MT5FleetPanel::onFleetRowClicked);
    splitter->addWidget(ea_table_);

    // Chart detail
    chart_panel_ = new QWidget; auto* cl = new QVBoxLayout(chart_panel_); cl->setContentsMargins(8,4,8,4);
    auto* cht = new QHBoxLayout;
    chart_title_ = new QLabel("Select an EA to view chart");
    chart_info_ = new QLabel(""); chart_close_ = new QPushButton("✕"); chart_close_->setFixedSize(22,22);
    chart_close_->setStyleSheet("background:transparent;color:#FF4444;border:none;font-size:12px;");
    connect(chart_close_,&QPushButton::clicked,this,[this](){ chart_panel_->hide(); selected_symbol_.clear(); });
    cht->addWidget(chart_title_,1); cht->addWidget(chart_info_); cht->addWidget(chart_close_); cl->addLayout(cht);
    chart_view_ = new QChartView; chart_view_->setRenderHint(QPainter::Antialiasing); chart_view_->setMinimumHeight(230);
    cl->addWidget(chart_view_,1); chart_panel_->hide();
    splitter->addWidget(chart_panel_); splitter->setStretchFactor(0,1); splitter->setStretchFactor(1,2);
    evl->addWidget(splitter,1);
    content_stack_->addWidget(ea_view_);

    // Page 1-3: Market tabs (Crypto, Forex, Stocks)
    market_tabs_ = new QTabWidget;
    auto mkMkt = [&](const QStringList& syms, const QString& name) {
        auto* tab = new QWidget; auto* vl = new QVBoxLayout(tab); vl->setContentsMargins(0,0,0,0);
        auto* tbl = new QTableWidget(0,6,tab);
        tbl->setHorizontalHeaderLabels({"Symbol","Price","24h","High","Low","Chart"});
        tbl->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
        tbl->setSelectionBehavior(QAbstractItemView::SelectRows); tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tbl->setStyleSheet(ea_table_->styleSheet());
        connect(tbl,&QTableWidget::cellClicked,this,[this,tbl](int r,int c){
            if(c!=5) return; auto* it=tbl->item(r,0); if(!it) return;
            selected_symbol_=it->text(); chart_title_->setText(it->text());
            chart_panel_->show(); updateChart(it->text());
        });
        vl->addWidget(tbl,1);
        market_tabs_->addTab(tab, name);
        market_tables_.append(tbl);
    };
    mkMkt(kCryptoS, "Crypto");
    mkMkt(kForexS, "Forex");
    mkMkt(kStockS, "Stocks");
    content_stack_->addWidget(market_tabs_);

    // Page 4: Quick Trade
    auto* tradeTab = new QWidget; auto* tl = new QVBoxLayout(tradeTab); tl->setContentsMargins(12,12,12,12);
    auto* symRow = new QHBoxLayout;
    auto* symCb = new QComboBox; symCb->addItems(kCryptoS+kForexS+kStockS);
    symRow->addWidget(new QLabel("Symbol:")); symRow->addWidget(symCb); symRow->addStretch(); tl->addLayout(symRow);
    auto* priceLbl = new QLabel("--"); priceLbl->setStyleSheet("font-size:18px;font-weight:700;color:#FFF;");
    tl->addWidget(priceLbl);
    auto* amt = new QLineEdit; amt->setPlaceholderText("Amount");
    auto* prc = new QLineEdit; prc->setPlaceholderText("Limit (Market)");
    tl->addWidget(amt); tl->addWidget(prc);
    auto* btns = new QHBoxLayout;
    auto* buy = new QPushButton("BUY"); buy->setStyleSheet("QPushButton{background:#22c55e;color:#FFF;font-weight:700;padding:8px 24px;border:none;border-radius:4px;}");
    auto* sell = new QPushButton("SELL"); sell->setStyleSheet("QPushButton{background:#ef4444;color:#FFF;font-weight:700;padding:8px 24px;border:none;border-radius:4px;}");
    btns->addWidget(buy); btns->addWidget(sell); btns->addStretch(); tl->addLayout(btns);

    connect(symCb,&QComboBox::currentTextChanged,this,[this,priceLbl](const QString& sym){
        HttpClient::instance().get(api_base_+"/mt5/market/ohlc?symbol="+sym+"&timeframe=H1&count=1",
            [this,sym,priceLbl](Result<QJsonDocument> r){
                if(r.is_err()) return;
                auto d=r.value().object()["data"].toArray();
                if(!d.isEmpty()) priceLbl->setText(QString("$%1").arg(d.last().toObject()["close"].toDouble(),0,'f',2));
            },this);
    });

    content_stack_->addWidget(tradeTab);

    root->addWidget(content_stack_, 1);

    // ── Status Bar (Portfolio-style) ──
    status_bar_ = new QWidget; status_bar_->setFixedHeight(22);
    status_bar_->setStyleSheet(QString("background:%1; border-top:1px solid %2;").arg(BG_RAISED(), BORDER_DIM()));
    auto* sbl2 = new QHBoxLayout(status_bar_); sbl2->setContentsMargins(12,0,12,0);
    status_engine_ = new QLabel("BRIDGE v1.0");
    status_engine_->setStyleSheet(QString("color:%1;font-size:8px;font-weight:700;").arg(CYAN_HEX));
    sbl2->addWidget(status_engine_);
    sbl2->addStretch();
    status_time_ = new QLabel(QDateTime::currentDateTime().toString("HH:mm:ss"));
    status_time_->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    sbl2->addWidget(status_time_);
    auto* refreshTimer = new QTimer(this);
    connect(refreshTimer,&QTimer::timeout,this,[this](){ status_time_->setText(QDateTime::currentDateTime().toString("HH:mm:ss")); });
    refreshTimer->start(1000);
    root->addWidget(status_bar_);
}

void MT5FleetPanel::refreshFleet() { refreshEAs(); updateStats(); }

void MT5FleetPanel::onFleetRowClicked(int row, int) {
    auto* it=ea_table_->item(row,0); if(!it) return;
    QString sym=it->data(Qt::UserRole+1).toString();
    if(sym.isEmpty()) return;
    selected_symbol_=sym, chart_title_->setText(sym);
    chart_panel_->show(); updateChart(sym);
}

void MT5FleetPanel::onTabChanged(int idx) {
    if(idx>=1 && idx<=3) {
        int ti = idx-1;
        if(ti<market_tables_.size()) {
            auto* tbl = market_tables_[ti];
            static const QStringList allSyms[] = {kCryptoS, kForexS, kStockS};
            if(ti<3) for(const auto& sym : allSyms[ti]) {
                HttpClient::instance().get(api_base_+"/mt5/market/ohlc?symbol="+sym+"&timeframe=H1&count=5",
                    [this,tbl,sym](Result<QJsonDocument> r){
                        if(r.is_err()) return;
                        auto data=r.value().object()["data"].toArray(); if(data.isEmpty()) return;
                        auto last=data.last().toObject(), first=data.first().toObject();
                        double p=last["close"].toDouble(),o=first["open"].toDouble(),hi=last["high"].toDouble(),lo=last["low"].toDouble(),chg=o>0?((p/o)-1)*100:0;
                        int row=-1;
                        for(int i=0;i<tbl->rowCount();++i) if(tbl->item(i,0)&&tbl->item(i,0)->text()==sym){row=i;break;}
                        if(row<0){row=tbl->rowCount();tbl->setRowCount(row+1);}
                        tbl->setItem(row,0,new QTableWidgetItem(sym));
                        tbl->setItem(row,1,new QTableWidgetItem(QString("$%1").arg(p,0,'f',p<100?4:2)));
                        auto* ci=new QTableWidgetItem(QString("%1%").arg(chg,0,'f',2));
                        ci->setForeground(chg>=0?QColor("#22c55e"):QColor("#ef4444"));
                        tbl->setItem(row,2,ci);
                        tbl->setItem(row,3,new QTableWidgetItem(QString("$%1").arg(hi,0,'f',2)));
                        tbl->setItem(row,4,new QTableWidgetItem(QString("$%1").arg(lo,0,'f',2)));
                        auto* cb=new QTableWidgetItem("📈"); cb->setData(Qt::UserRole,sym);
                        tbl->setItem(row,5,cb);
                    },this);
            }
        }
    }
}

void MT5FleetPanel::refreshEAs() {
    HttpClient::instance().get(api_base_+"/mt5/ea/list",[this](Result<QJsonDocument> r){
        if(r.is_err()) return;
        auto arr=r.value().object()["data"].toArray();
        ea_table_->setRowCount(arr.size());
        double totalBal=0, totalPnL=0;
        for(int i=0;i<arr.size();++i){
            auto o=arr.at(i).toObject();
            auto* ni=new QTableWidgetItem(o["ea_name"].toString());
            ni->setData(Qt::UserRole,o["eid"].toString()); ni->setData(Qt::UserRole+1,o["symbol"].toString());
            ea_table_->setItem(i,0,ni);
            ea_table_->setItem(i,1,new QTableWidgetItem(o["symbol"].toString()));
            ea_table_->setItem(i,2,new QTableWidgetItem(o["timeframe"].toString()));
            ea_table_->setItem(i,3,new QTableWidgetItem(QString::number(o["magic_number"].toInt())));
            auto* si=new QTableWidgetItem(o["status"].toString().toUpper());
            si->setForeground(o["status"].toString()=="running"?QColor("#22c55e"):QColor("#ef4444"));
            ea_table_->setItem(i,4,si);
            double bal=o["balance"].toDouble(), pnl=o["pnl"].toDouble(); totalBal+=bal; totalPnL+=pnl;
            ea_table_->setItem(i,5,new QTableWidgetItem(QString("$%1").arg(bal,0,'f',2)));
            ea_table_->setItem(i,6,new QTableWidgetItem(QString("$%1").arg(o["equity"].toDouble(),0,'f',2)));
            auto* ab=new QTableWidgetItem("📈"); ab->setData(Qt::UserRole,o["eid"].toString());
            ea_table_->setItem(i,7,ab);
        }
        stat_connected_->setText(QString("%1 EA").arg(arr.size()));
        stat_balance_->setText(QString("$%1").arg(totalBal,0,'f',0));
        stat_pnl_->setText(QString("$%1").arg(totalPnL,0,'f',2));
        stat_pnl_->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(totalPnL>=0?"#22c55e":"#ef4444"));
    },this);
}

void MT5FleetPanel::updateStats() {
    // Update time and count on refresh
    count_label_ = stat_connected_;
}

void MT5FleetPanel::updatePriceView(const QString& symbol) {}
void MT5FleetPanel::updateChart(const QString& symbol) {
    HttpClient::instance().get(api_base_+"/mt5/market/ohlc?symbol="+symbol+"&timeframe=H1&count=120",
        [this,symbol](Result<QJsonDocument> r){
            if(r.is_err()){chart_info_->setText("Failed");return;}
            auto candles=r.value().object()["data"].toArray(); if(candles.isEmpty()){chart_info_->setText("No data");return;}
            clearChart();
            auto* cs=new QCandlestickSeries(); cs->setName(symbol);
            cs->setIncreasingColor(QColor("#26a69a")); cs->setDecreasingColor(QColor("#ef5350"));
            auto* e9=new QLineSeries(); e9->setName("EMA 9"); e9->setPen(QPen(QColor("#FFC400"),1.5));
            auto* e21=new QLineSeries(); e21->setName("EMA 21"); e21->setPen(QPen(QColor("#00E5FF"),1.5));
            double minP=1e10,maxP=0; qint64 minT=9e18,maxT=0;
            for(auto cv:candles){
                auto c=cv.toObject(); qint64 ts=(qint64)(c["time"].toDouble())*1000;
                double o=c["open"].toDouble(),h=c["high"].toDouble(),l=c["low"].toDouble(),cl=c["close"].toDouble();
                cs->append(new QCandlestickSet(o,h,l,cl,ts));
                minP=qMin(minP,l); maxP=qMax(maxP,h); minT=qMin(minT,ts); maxT=qMax(maxT,ts);
                if(!c["ema9"].isNull()) e9->append(ts,c["ema9"].toDouble());
                if(!c["ema21"].isNull()) e21->append(ts,c["ema21"].toDouble());
            }
            double pad=(maxP-minP)*0.05; minP-=pad; maxP+=pad;
            chart_=new QChart();
            chart_->setAnimationOptions(QChart::SeriesAnimations);
            chart_->setBackgroundBrush(QColor(BG_RAISED()));
            chart_->legend()->setVisible(true); chart_->legend()->setAlignment(Qt::AlignTop);
            chart_->legend()->setLabelColor(QColor(TEXT_PRIMARY()));
            auto lf=chart_->legend()->font(); lf.setPointSize(9); chart_->legend()->setFont(lf);
            chart_->addSeries(cs); chart_->addSeries(e9); chart_->addSeries(e21);
            auto* ax=new QDateTimeAxis(); ax->setFormat("HH:mm\nMMM dd"); ax->setTickCount(8);
            ax->setLabelsColor(QColor(TEXT_TERTIARY()));
            ax->setRange(QDateTime::fromMSecsSinceEpoch(minT),QDateTime::fromMSecsSinceEpoch(maxT));
            chart_->addAxis(ax,Qt::AlignBottom);
            auto* ay=new QValueAxis(); ay->setLabelFormat("$%.2f"); ay->setTickCount(8);
            ay->setLabelsColor(QColor(TEXT_TERTIARY())); ay->setRange(minP,maxP);
            ay->setTitleText(symbol); ay->setTitleBrush(QBrush(QColor(TEXT_TERTIARY())));
            chart_->addAxis(ay,Qt::AlignLeft);
            cs->attachAxis(ax); cs->attachAxis(ay); e9->attachAxis(ax); e9->attachAxis(ay);
            chart_view_->setChart(chart_);
            chart_info_->setText(QString("$%1").arg(candles.last().toObject()["close"].toDouble(),0,'f',2));
        },this);
}

void MT5FleetPanel::clearChart() {
    if(chart_){chart_->removeAllSeries();chart_view_->setChart(new QChart());delete chart_;chart_=nullptr;}
}

} // namespace fincept::screens
