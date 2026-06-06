#include "screens/algo_trading/StrategyScannerPanel.h"
#include "ui/theme/Theme.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QHeaderView>
#include <QMessageBox>

namespace fincept::screens {

StrategyScannerPanel::StrategyScannerPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void StrategyScannerPanel::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(8,8,8,8);

    auto* hl = new QHBoxLayout();
    auto* title = new QLabel("<b style='font-size:14px;'>Strategy Scanner</b>", this);
    hl->addWidget(title);
    scan_btn_ = new QPushButton("Start Scan", this);
    scan_btn_->setStyleSheet("background:#22c55e;color:#080808;font-weight:700;padding:8px 20px;border-radius:4px;");
    connect(scan_btn_, &QPushButton::clicked, this, &StrategyScannerPanel::on_scan);
    hl->addWidget(scan_btn_);
    status_lbl_ = new QLabel("Ready", this);
    status_lbl_->setStyleSheet("color:#808080;font-size:11px;");
    hl->addWidget(status_lbl_);
    root->addLayout(hl);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 0);
    progress_->setVisible(false);
    root->addWidget(progress_);

    table_ = new QTableWidget(0, 8, this);
    table_->setHorizontalHeaderLabels({"Rank", "Symbol", "Strategy", "TF", "Return%", "PF", "Sharpe", "Trades"});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setAlternatingRowColors(true);
    root->addWidget(table_, 1);
}

void StrategyScannerPanel::on_scan() {
    scan_btn_->setEnabled(false);
    progress_->setVisible(true);
    status_lbl_->setText("Scanning all symbols × strategies...");

    HttpClient::instance().post("http://localhost:8150/backtest/scan", QJsonObject(),
        [this](Result<QJsonDocument> r) {
            scan_btn_->setEnabled(true);
            progress_->setVisible(false);
            if (!r.is_ok()) {
                status_lbl_->setText("Scan failed — backend unreachable");
                return;
            }
            auto obj = r.value().object();
            if (!obj["success"].toBool()) {
                status_lbl_->setText("Scan error: " + obj["error"].toString());
                return;
            }
            auto data = obj["data"].toObject();
            int total = data["total_tested"].toInt();
            auto top = data["top_results"].toArray();
            last_results_ = top;
            display_results(top);
            status_lbl_->setText(QString("Tested %1 combinations — showing top %2").arg(total).arg(top.size()));
        }, this);
}

void StrategyScannerPanel::display_results(const QJsonArray& results) {
    table_->setRowCount(results.size());
    for (int i = 0; i < results.size(); ++i) {
        auto r = results[i].toObject();
        table_->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        table_->setItem(i, 1, new QTableWidgetItem(r["symbol"].toString()));
        table_->setItem(i, 2, new QTableWidgetItem(r["strategy"].toString()));
        table_->setItem(i, 3, new QTableWidgetItem(r["timeframe"].toString()));

        double ret = r["total_return"].toDouble();
        auto* ret_item = new QTableWidgetItem(QString("%1%").arg(ret, 0, 'f', 2));
        ret_item->setForeground(ret >= 0 ? QColor("#22c55e") : QColor("#ef4444"));
        table_->setItem(i, 4, ret_item);

        table_->setItem(i, 5, new QTableWidgetItem(QString::number(r["profit_factor"].toDouble(), 'f', 2)));
        table_->setItem(i, 6, new QTableWidgetItem(QString::number(r["sharpe"].toDouble(), 'f', 2)));
        table_->setItem(i, 7, new QTableWidgetItem(QString::number(r["total_trades"].toInt())));
    }
}

void StrategyScannerPanel::on_deploy(int row) {
    if (row < 0 || row >= last_results_.size()) return;
    auto r = last_results_[row].toObject();

    QMessageBox::information(this, "Deploy",
        QString("Deploying %1 on %2 %3\nSubmit to compile endpoint?")
            .arg(r["strategy"].toString(), r["symbol"].toString(), r["timeframe"].toString()));
}

} // namespace fincept::screens
