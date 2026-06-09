// MT5FleetMarketWatchPanel.cpp — Industrial-grade Market Watch
#include "screens/algo_trading/MT5FleetMarketWatchPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QShowEvent>
#include <QApplication>

namespace fincept::screens {

static const int POLL_MS = 1500;
static const int MAX_SYMBOLS_PER_POLL = 3;

MT5FleetMarketWatchPanel::MT5FleetMarketWatchPanel(QWidget* parent) : QWidget(parent) {
    forex_symbols_ = {"EURUSD","GBPUSD","USDJPY","USDCHF","USDCAD","AUDUSD","NZDUSD",
                      "EURGBP","EURJPY","GBPJPY","AUDJPY","CHFJPY","NZDJPY","EURAUD",
                      "GBPAUD","AUDCAD","AUDCHF","EURCHF","EURCAD","EURNZD","GBPCHF",
                      "GBPNZD","NZDCAD","NZDCAD","CADCHF","CADJPY"};
    crypto_symbols_ = {"BTCUSD","ETHUSD","XRPUSD","LTCUSD","BCHUSD","ADAUSD",
                       "DOTUSD","LINKUSD","XLMUSD","DOGEUSD","SOLUSD","MATICUSD"};
    stock_symbols_ = {"AAPL","MSFT","GOOGL","AMZN","TSLA","META","NVDA","JPM",
                      "V","JNJ","WMT","PG","MA","UNH","HD","BAC","DIS","ADBE",
                      "NFLX","CRM","INTC","AMD","PYPL","QCOM"};
    commodity_symbols_ = {"XAUUSD","XAGUSD","XPTUSD","XPDUSD","USOIL","UKOIL",
                          "USDCAD","AUDUSD","NZDUSD"};

    all_symbols_ << forex_symbols_ << crypto_symbols_ << stock_symbols_ << commodity_symbols_;

    build_ui();
    apply_theme();
    repopulate_tree();
}

MT5FleetMarketWatchPanel::~MT5FleetMarketWatchPanel() = default;

void MT5FleetMarketWatchPanel::setSymbols(const QStringList& symbols) {
    if (!symbols.isEmpty()) {
        all_symbols_ = symbols;
        repopulate_tree();
    }
}

void MT5FleetMarketWatchPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("marketWatchHeader");
    header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10,0,10,0);
    hl->setSpacing(6);

    auto* title = new QLabel("MARKET WATCH", header);
    title->setObjectName("marketWatchTitle");
    hl->addWidget(title);
    hl->addStretch();

    status_label_ = new QLabel("◌ idle", header);
    status_label_->setObjectName("marketWatchStatus");
    hl->addWidget(status_label_);

    timer_label_ = new QLabel("", header);
    timer_label_->setObjectName("marketWatchTimer");
    hl->addWidget(timer_label_);

    root->addWidget(header);

    // Filter bar
    auto* filter_bar = new QWidget(this);
    filter_bar->setObjectName("marketWatchFilter");
    filter_bar->setFixedHeight(32);
    auto* fl = new QHBoxLayout(filter_bar);
    fl->setContentsMargins(8,2,8,2);
    fl->setSpacing(4);

    search_input_ = new QLineEdit(filter_bar);
    search_input_->setObjectName("marketWatchSearch");
    search_input_->setPlaceholderText("🔍 Search symbols...");
    search_input_->setClearButtonEnabled(true);
    connect(search_input_, &QLineEdit::textChanged, this, &MT5FleetMarketWatchPanel::on_search_changed);
    fl->addWidget(search_input_, 1);

    filter_combo_ = new QComboBox(filter_bar);
    filter_combo_->setObjectName("marketWatchFilterCombo");
    filter_combo_->addItems({"All","Forex","Crypto","Stocks","Commodities"});
    connect(filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetMarketWatchPanel::on_filter_changed);
    fl->addWidget(filter_combo_);

    root->addWidget(filter_bar);

    // Tree widget
    tree_ = new QTreeWidget(this);
    tree_->setObjectName("marketWatchTree");
    tree_->setColumnCount(8);
    tree_->setHeaderLabels({"Symbol","Bid","Ask","Spread","Change","Change%","High","Low"});
    tree_->setRootIsDecorated(true);
    tree_->setAlternatingRowColors(false);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setAnimated(true);
    tree_->setIndentation(16);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(5, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(6, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(7, QHeaderView::Stretch);
    tree_->setSortingEnabled(true);
    tree_->sortByColumn(0, Qt::AscendingOrder);
    connect(tree_, &QTreeWidget::itemClicked, this, &MT5FleetMarketWatchPanel::on_item_clicked);
    root->addWidget(tree_, 1);

    // Timer for polling
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MT5FleetMarketWatchPanel::refresh_all);
}

void MT5FleetMarketWatchPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#marketWatchHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#marketWatchTitle{color:%3;font-size:11px;font-weight:700;letter-spacing:1.5px;background:transparent;}"
        "QLabel#marketWatchStatus{color:%4;font-size:9px;background:transparent;}"
        "QLabel#marketWatchTimer{color:%4;font-size:9px;background:transparent;}"
        "QWidget#marketWatchFilter{background:%1;border-bottom:1px solid %2;}"
        "QLineEdit#marketWatchSearch{background:%5;color:%6;border:1px solid %2;padding:2px 6px;font-size:11px;border-radius:3px;}"
        "QComboBox#marketWatchFilterCombo{background:%5;color:%6;border:1px solid %2;padding:2px 6px;font-size:10px;}"
        "QTreeWidget#marketWatchTree{background:%7;color:%6;border:none;font-size:11px;}"
        "QTreeWidget#marketWatchTree::item{padding:3px 4px;border-bottom:1px solid %8;}"
        "QTreeWidget#marketWatchTree::item:selected{background:%3;color:%7;}"
        "QTreeWidget#marketWatchTree QHeaderView::section{background:%1;color:%4;border:none;border-right:1px solid %2;padding:4px;font-size:9px;font-weight:700;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
          ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
}

void MT5FleetMarketWatchPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!is_active_) {
        is_active_ = true;
        refresh_count_ = 0;
        status_label_->setText("● polling");
        timer_->start(POLL_MS);
        refresh_all();
    }
}

void MT5FleetMarketWatchPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (is_active_) {
        is_active_ = false;
        timer_->stop();
        status_label_->setText("◌ idle");
    }
}

void MT5FleetMarketWatchPanel::on_search_changed(const QString& text) {
    search_filter_ = text.trimmed().toUpper();
    repopulate_tree();
}

void MT5FleetMarketWatchPanel::on_filter_changed(int idx) {
    switch (idx) {
        case 0: category_filter_ = "All"; break;
        case 1: category_filter_ = "Forex"; break;
        case 2: category_filter_ = "Crypto"; break;
        case 3: category_filter_ = "Stocks"; break;
        case 4: category_filter_ = "Commodities"; break;
    }
    repopulate_tree();
}

void MT5FleetMarketWatchPanel::on_item_clicked(QTreeWidgetItem* item, int) {
    if (!item || !item->parent()) return;
    QString symbol = item->text(0);
    if (!symbol.isEmpty()) {
        current_symbol_ = symbol;
        emit symbolSelected(symbol);
    }
}

void MT5FleetMarketWatchPanel::refresh_all() {
    if (all_symbols_.isEmpty()) return;

    // Staggered fetching: fetch MAX_SYMBOLS_PER_POLL per tick
    for (int i = 0; i < MAX_SYMBOLS_PER_POLL; ++i) {
        int idx = (fetch_index_ + i) % all_symbols_.size();
        fetch_quote(all_symbols_[idx]);
    }
    fetch_index_ = (fetch_index_ + MAX_SYMBOLS_PER_POLL) % all_symbols_.size();
    refresh_count_++;
    timer_label_->setText(QString("%1/%2").arg(refresh_count_).arg(all_symbols_.size()));

    // After 10 seconds, re-poll all symbols (staggered)
    if (refresh_count_ * POLL_MS / 1000 > 8) {
        fetch_index_ = 0;
        refresh_count_ = 0;
    }
}

void MT5FleetMarketWatchPanel::fetch_quote(const QString& symbol) {
    if (active_requests_.contains(symbol)) return;
    active_requests_.insert(symbol);

    HttpClient::instance().get(
        QString("/mt5/market/orderbook?symbol=%1").arg(symbol),
        [this, symbol](Result<QJsonDocument> r) {
            active_requests_.remove(symbol);
            if (r.is_err()) return;

            auto obj = r.value().object();
            double last_price = obj["last_price"].toDouble();
            double spread = obj["spread"].toDouble();

            // Get bid/ask from first level
            auto bids = obj["bids"].toArray();
            auto asks = obj["asks"].toArray();
            double bid = 0, ask = 0;
            if (!bids.isEmpty()) bid = bids[0].toObject()["price"].toDouble();
            if (!asks.isEmpty()) ask = asks[0].toObject()["price"].toDouble();

            // Also fetch OHLC for high/low
            HttpClient::instance().get(
                QString("/mt5/market/ohlc?symbol=%1&timeframe=D1&count=1").arg(symbol),
                [this, symbol, last_price, spread, bid, ask](Result<QJsonDocument> r2) {
                    if (r2.is_err()) return;
                    auto arr = r2.value().object()["data"].toArray();
                    MarketWatchEntry entry;
                    entry.symbol = symbol;
                    entry.bid = bid;
                    entry.ask = ask;
                    entry.spread = spread;
                    entry.last = last_price;

                    if (!arr.isEmpty()) {
                        auto ohlc = arr.last().toObject();
                        entry.high = ohlc["high"].toDouble();
                        entry.low = ohlc["low"].toDouble();
                        double open = ohlc["open"].toDouble();
                        entry.change = last_price - open;
                        entry.change_pct = open > 0 ? (entry.change / open) * 100 : 0;
                    }

                    // Determine category
                    if (forex_symbols_.contains(symbol)) entry.category = "Forex";
                    else if (crypto_symbols_.contains(symbol)) entry.category = "Crypto";
                    else if (stock_symbols_.contains(symbol)) entry.category = "Stocks";
                    else if (commodity_symbols_.contains(symbol)) entry.category = "Commodities";
                    else entry.category = "Other";

                    entry.up = entry.change >= 0;
                    entry.changed = true;
                    update_entry(symbol, entry);
                }, this);
        }, this);
}

void MT5FleetMarketWatchPanel::update_entry(const QString& symbol, MarketWatchEntry entry) {
    bool is_new = !entries_.contains(symbol);
    if (!is_new) {
        auto& old = entries_[symbol];
        entry.changed = qAbs(old.bid - entry.bid) > 0.001 || qAbs(old.ask - entry.ask) > 0.001;
        entry.up = entry.change >= 0;
    }
    entries_[symbol] = entry;

    // Find the item and update it
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        auto* cat_item = tree_->topLevelItem(i);
        for (int j = 0; j < cat_item->childCount(); ++j) {
            auto* item = cat_item->child(j);
            if (item->text(0) == symbol) {
                auto prev_bg = item->background(1);

                item->setText(1, entry.bid > 0 ? QString("$%1").arg(entry.bid, 0, 'f', (entry.bid < 10 ? 5 : 2)) : "—");
                item->setText(2, entry.ask > 0 ? QString("$%1").arg(entry.ask, 0, 'f', (entry.ask < 10 ? 5 : 2)) : "—");
                item->setText(3, entry.spread > 0 ? QString::number(entry.spread, 'f', 1) : "—");

                if (entry.last > 0) {
                    item->setText(4, QString("%1%2").arg(change_arrow(entry.change)).arg(qAbs(entry.change), 0, 'f', 2));
                    item->setText(5, QString("%1%2%").arg(entry.change >= 0 ? "+" : "").arg(entry.change_pct, 0, 'f', 2));
                    item->setForeground(4, entry.up ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE()));
                    item->setForeground(5, entry.up ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE()));
                }
                item->setText(6, entry.high > 0 ? QString("$%1").arg(entry.high, 0, 'f', 2) : "—");
                item->setText(7, entry.low > 0 ? QString("$%1").arg(entry.low, 0, 'f', 2) : "—");

                // Flash effect on change
                if (entry.changed) {
                    QColor flash = entry.up ? QColor(0, 255, 100, 30) : QColor(255, 50, 50, 30);
                    item->setBackground(1, flash);
                    item->setBackground(2, flash);
                    QTimer::singleShot(300, this, [this, item]() {
                        if (item) {
                            item->setBackground(1, QColor());
                            item->setBackground(2, QColor());
                        }
                    });
                }
                return;
            }
        }
    }
}

void MT5FleetMarketWatchPanel::repopulate_tree() {
    tree_->clear();

    QStringList categories;
    if (category_filter_ == "All" || category_filter_ == "Forex") categories << "Forex";
    if (category_filter_ == "All" || category_filter_ == "Crypto") categories << "Crypto";
    if (category_filter_ == "All" || category_filter_ == "Stocks") categories << "Stocks";
    if (category_filter_ == "All" || category_filter_ == "Commodities") categories << "Commodities";

    for (const auto& cat : categories) {
        QStringList* sym_list = nullptr;
        if (cat == "Forex") sym_list = &forex_symbols_;
        else if (cat == "Crypto") sym_list = &crypto_symbols_;
        else if (cat == "Stocks") sym_list = &stock_symbols_;
        else if (cat == "Commodities") sym_list = &commodity_symbols_;
        if (!sym_list) continue;

        auto* cat_item = new QTreeWidgetItem(tree_);
        cat_item->setText(0, category_icon(cat) + " " + cat);
        cat_item->setFlags(cat_item->flags() & ~Qt::ItemIsSelectable);
        cat_item->setForeground(0, QColor(ui::colors::AMBER()));
        QFont cat_font = cat_item->font(0);
        cat_font.setBold(true);
        cat_font.setPointSize(10);
        cat_item->setFont(0, cat_font);
        cat_item->setFirstColumnSpanned(true);

        for (const auto& sym : *sym_list) {
            if (!search_filter_.isEmpty() && !sym.contains(search_filter_)) continue;

            auto* item = new QTreeWidgetItem(cat_item);
            item->setText(0, sym);
            item->setForeground(0, QColor(ui::colors::TEXT_PRIMARY()));
            item->setText(1, "—");
            item->setText(2, "—");
            item->setText(3, "—");
            item->setText(4, "—");
            item->setText(5, "—");
            item->setText(6, "—");
            item->setText(7, "—");
        }

        cat_item->setExpanded(true);
    }
}

QString MT5FleetMarketWatchPanel::category_icon(const QString& cat) const {
    if (cat == "Forex") return QString::fromUtf8("\xF0\x9F\x92\xB1"); // 💱
    if (cat == "Crypto") return QString::fromUtf8("\xF0\x9F\x94\x97"); // 🔗
    if (cat == "Stocks") return QString::fromUtf8("\xF0\x9F\x93\x88"); // 📈
    if (cat == "Commodities") return QString::fromUtf8("\xF0\x9F\x9B\xA2"); // 🛢
    return "";
}

QString MT5FleetMarketWatchPanel::change_arrow(double change) const {
    return change > 0 ? QString::fromUtf8("\xE2\x96\xB2") :  // ▲
           change < 0 ? QString::fromUtf8("\xE2\x96\xBC") :  // ▼
                        "";
}

} // namespace fincept::screens
