#include "ui/charts/StockScreenerWidget.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "network/http/HttpClient.h"
#include "core/config/AppConfig.h"

namespace fincept::ui {

StockScreenerWidget::StockScreenerWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* top = new QHBoxLayout();
    market_combo_ = new QComboBox();
    market_combo_->addItems({"US", "INDIA", "EUROPE", "CRYPTO"});
    top->addWidget(new QLabel("Market:"));
    top->addWidget(market_combo_);

    filter_input_ = new QLineEdit();
    filter_input_->setPlaceholderText("Filter symbols...");
    connect(filter_input_, &QLineEdit::textChanged, this, &StockScreenerWidget::apply_filter);
    top->addWidget(filter_input_);

    refresh_btn_ = new QPushButton("Refresh");
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() { fetch_data(); });
    top->addWidget(refresh_btn_);

    count_label_ = new QLabel("0 stocks");
    top->addWidget(count_label_);
    layout->addLayout(top);

    table_ = new QTableWidget();
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({"Symbol", "Price", "Chg%", "Volume", "RSI", "M.Cap"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setAlternatingRowColors(true);
    connect(table_, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row >= 0 && row < data_.size())
            emit symbol_selected(data_[row].symbol);
    });
    layout->addWidget(table_);

    fetch_timer_ = new QTimer(this);
    fetch_timer_->setInterval(60000);
    connect(fetch_timer_, &QTimer::timeout, this, &StockScreenerWidget::fetch_data);

    load_data();
}

void StockScreenerWidget::load_data(const QString& market) {
    Q_UNUSED(market)
    fetch_data();
}

void StockScreenerWidget::fetch_data() {
    QString url = fincept::AppConfig::instance().api_base_url() + "/market/screener";
    QJsonObject body;
    body["market"] = market_combo_->currentText().toLower();
    HttpClient::instance().post(url, body, [this](Result<QJsonDocument> result) {
        if (!result || !result.value().object()["success"].toBool()) return;
        auto arr = result.value().object()["data"].toObject()["stocks"].toArray();
        data_.clear();
        for (const auto& v : arr) {
            auto o = v.toObject();
            StockRow r;
            r.symbol = o["symbol"].toString();
            r.price = o["price"].toDouble();
            r.change = o["change_pct"].toDouble();
            r.volume = o["volume"].toDouble();
            r.rsi = o["rsi"].toDouble(50);
            r.mcap = o["market_cap"].toDouble();
            data_.append(r);
        }
        apply_filter();
    }, this);
}

void StockScreenerWidget::apply_filter() {
    QString filter = filter_input_->text().toUpper();
    table_->setRowCount(0);
    int count = 0;
    for (const auto& r : data_) {
        if (!filter.isEmpty() && !r.symbol.contains(filter)) continue;
        table_->insertRow(count);
        populate_row(count, r.symbol, r.price, r.change, r.volume, r.rsi, r.mcap);
        count++;
    }
    count_label_->setText(QString("%1 stocks").arg(count));
}

void StockScreenerWidget::populate_row(int row, const QString& symbol, double price,
                                        double change, double volume, double rsi, double mcap) {
    auto* sym = new QTableWidgetItem(symbol);
    sym->setForeground(QColor("#f59e0b"));
    table_->setItem(row, 0, sym);
    table_->setItem(row, 1, new QTableWidgetItem(QString::number(price, 'f', 2)));
    auto* chg = new QTableWidgetItem(QString("%1%").arg(change, 0, 'f', 1));
    chg->setForeground(change >= 0 ? QColor("#089981") : QColor("#f23645"));
    table_->setItem(row, 2, chg);
    table_->setItem(row, 3, new QTableWidgetItem(QString::number(volume / 1e6, 'f', 1) + "M"));
    table_->setItem(row, 4, new QTableWidgetItem(QString::number(rsi, 'f', 0)));
    QString mcap_s = mcap > 1e12 ? QString("%1T").arg(mcap / 1e12, 0, 'f', 1)
                   : mcap > 1e9 ? QString("%1B").arg(mcap / 1e9, 0, 'f', 1)
                   : QString("%1M").arg(mcap / 1e6, 0, 'f', 0);
    table_->setItem(row, 5, new QTableWidgetItem(mcap_s));
}

} // namespace fincept::ui
