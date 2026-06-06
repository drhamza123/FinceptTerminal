// MT5FleetSignalsPanel.cpp — Copy Trading / Trading Signals
#include "screens/algo_trading/MT5FleetSignalsPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>

namespace fincept::screens {

MT5FleetSignalsPanel::MT5FleetSignalsPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_theme();
    
    // Auto-refresh every 10 seconds
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MT5FleetSignalsPanel::refresh_signals);
    timer->start(10000);
    
    refresh_signals();
}

MT5FleetSignalsPanel::~MT5FleetSignalsPanel() = default;

void MT5FleetSignalsPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("signalsHeader");
    header->setFixedHeight(40);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(12, 0, 12, 0);
    h_layout->setSpacing(8);

    auto* title_label = new QLabel("TRADING SIGNALS", header);
    title_label->setObjectName("signalsTitleLabel");
    h_layout->addWidget(title_label);

    h_layout->addStretch();

    filter_combo_ = new QComboBox(header);
    filter_combo_->setObjectName("signalsFilterCombo");
    filter_combo_->addItems({"All Signals", "Top Performers", "Forex", "Stocks", "Crypto", "Commodities"});
    connect(filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MT5FleetSignalsPanel::on_filter_changed);
    h_layout->addWidget(filter_combo_);

    root->addWidget(header);

    // Signals table
    signals_table_ = new QTableWidget(0, 8, this);
    signals_table_->setObjectName("signalsTable");
    signals_table_->setHorizontalHeaderLabels({
        "Provider", "Symbol", "Signal", "Entry", "SL", "TP", "Win Rate", "Status"
    });
    signals_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    signals_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    signals_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    signals_table_->verticalHeader()->setVisible(false);
    root->addWidget(signals_table_, 1);

    // Bottom bar
    auto* bottom = new QWidget(this);
    bottom->setObjectName("signalsBottom");
    bottom->setFixedHeight(50);
    auto* b_layout = new QHBoxLayout(bottom);
    b_layout->setContentsMargins(12, 8, 12, 8);
    b_layout->setSpacing(12);

    status_label_ = new QLabel("Select a signal to copy", bottom);
    status_label_->setObjectName("signalsStatusLabel");
    b_layout->addWidget(status_label_, 1);

    copy_btn_ = new QPushButton("COPY SIGNAL", bottom);
    copy_btn_->setObjectName("signalsCopyBtn");
    copy_btn_->setFixedHeight(34);
    copy_btn_->setEnabled(false);
    connect(copy_btn_, &QPushButton::clicked, this, &MT5FleetSignalsPanel::on_copy_clicked);
    b_layout->addWidget(copy_btn_);

    root->addWidget(bottom);

    // Connect table selection
    connect(signals_table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        auto selected = signals_table_->selectedItems();
        copy_btn_->setEnabled(!selected.isEmpty());
        if (!selected.isEmpty()) {
            auto* provider_item = signals_table_->item(selected.first()->row(), 0);
            if (provider_item) {
                selected_signal_id_ = provider_item->text();
                status_label_->setText(QString("Selected: %1").arg(selected_signal_id_));
            }
        }
    });
}

void MT5FleetSignalsPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#signalsHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#signalsTitleLabel{color:%3;font-size:14px;font-weight:700;}"
        "QComboBox#signalsFilterCombo{background:%4;color:%3;border:1px solid %2;padding:4px 8px;min-width:120px;}"
        "QTableWidget#signalsTable{background:%5;color:%3;border:1px solid %2;}"
        "QTableWidget::item{padding:4px 8px;font-size:11px;}"
        "QWidget#signalsBottom{background:%1;border-top:1px solid %2;}"
        "QLabel#signalsStatusLabel{color:%6;font-size:11px;}"
        "QPushButton#signalsCopyBtn{background:%7;color:#FFF;border:none;font-size:12px;font-weight:700;padding:0 20px;}"
        "QPushButton#signalsCopyBtn:hover{background:#00B85C;}"
        "QPushButton#signalsCopyBtn:disabled{background:%8;color:%6;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_RAISED(), ui::colors::BG_BASE(), ui::colors::TEXT_TERTIARY(),
          ui::colors::POSITIVE(), ui::colors::BG_HOVER()));
}

void MT5FleetSignalsPanel::refresh_signals() {
    QString filter = filter_combo_->currentText();
    QString endpoint = "http://localhost:8150/mt5/signals";
    
    if (filter == "Top Performers") endpoint += "?filter=top";
    else if (filter == "Forex") endpoint += "?filter=forex";
    else if (filter == "Stocks") endpoint += "?filter=stocks";
    else if (filter == "Crypto") endpoint += "?filter=crypto";
    else if (filter == "Commodities") endpoint += "?filter=commodities";

    HttpClient::instance().get(endpoint, [this](Result<QJsonDocument> result) {
        if (result.is_err()) return;
        auto doc = result.value();
        auto signal_list = doc.object()["signals"].toArray();
        update_signals_table(signal_list);
    }, this);
}

void MT5FleetSignalsPanel::update_signals_table(const QJsonArray& signal_list) {
    signals_table_->setRowCount(signal_list.size());
    
    for (int i = 0; i < signal_list.size(); ++i) {
        auto sig = signal_list[i].toObject();
        
        signals_table_->setItem(i, 0, new QTableWidgetItem(sig["provider"].toString()));
        signals_table_->setItem(i, 1, new QTableWidgetItem(sig["symbol"].toString()));
        
        auto* signal_item = new QTableWidgetItem(sig["signal"].toString());
        if (sig["signal"].toString() == "BUY") {
            signal_item->setForeground(QColor(ui::colors::POSITIVE()));
        } else if (sig["signal"].toString() == "SELL") {
            signal_item->setForeground(QColor(ui::colors::NEGATIVE()));
        }
        signals_table_->setItem(i, 2, signal_item);
        
        signals_table_->setItem(i, 3, new QTableWidgetItem(QString("$%1").arg(sig["entry"].toDouble(), 0, 'f', 2)));
        signals_table_->setItem(i, 4, new QTableWidgetItem(QString("$%1").arg(sig["sl"].toDouble(), 0, 'f', 2)));
        signals_table_->setItem(i, 5, new QTableWidgetItem(QString("$%1").arg(sig["tp"].toDouble(), 0, 'f', 2)));
        signals_table_->setItem(i, 6, new QTableWidgetItem(QString("%1%").arg(sig["win_rate"].toDouble(), 0, 'f', 1)));
        
        auto* status_item = new QTableWidgetItem(sig["status"].toString());
        if (sig["status"].toString() == "Active") {
            status_item->setForeground(QColor(ui::colors::POSITIVE()));
        } else if (sig["status"].toString() == "Closed") {
            status_item->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        }
        signals_table_->setItem(i, 7, status_item);
    }
}

void MT5FleetSignalsPanel::on_copy_clicked() {
    if (selected_signal_id_.isEmpty()) return;
    
    auto reply = QMessageBox::question(
        this, "Copy Signal",
        QString("Copy all trades from %1?\n\nThis will automatically execute trades in your account.").arg(selected_signal_id_),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        HttpClient::instance().post(
            QString("http://localhost:8150/mt5/signals/copy"),
            QJsonObject{{"provider_id", selected_signal_id_}},
            [this](Result<QJsonDocument> result) {
                if (result.is_err()) {
                    QMessageBox::warning(this, "Error", "Failed to copy signal");
                    return;
                }
                QMessageBox::information(this, "Success", "Signal copied successfully!");
                status_label_->setText("Signal copied!");
            }, this);
    }
}

void MT5FleetSignalsPanel::on_filter_changed(int idx) {
    refresh_signals();
}

} // namespace fincept::screens
