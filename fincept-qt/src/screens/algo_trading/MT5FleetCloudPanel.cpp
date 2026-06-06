// MT5FleetCloudPanel.cpp — Cloud Strategy Optimization
#include "screens/algo_trading/MT5FleetCloudPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QTimer>

namespace fincept::screens {

MT5FleetCloudPanel::MT5FleetCloudPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_theme();
    
    // Auto-refresh every 5 seconds
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MT5FleetCloudPanel::refresh_optimizations);
    timer->start(5000);
    
    refresh_optimizations();
}

MT5FleetCloudPanel::~MT5FleetCloudPanel() = default;

void MT5FleetCloudPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // Configuration group
    auto* config_group = new QGroupBox("Optimization Configuration", this);
    config_group->setObjectName("cloudConfigGroup");
    auto* config_layout = new QFormLayout(config_group);
    config_layout->setSpacing(8);

    ea_combo_ = new QComboBox(config_group);
    ea_combo_->setObjectName("cloudEACombo");
    ea_combo_->addItems({"GoldEA", "GuardianBridge", "TrendFollower", "Scalper", "Custom EA"});
    config_layout->addRow("Expert Advisor:", ea_combo_);

    symbol_combo_ = new QComboBox(config_group);
    symbol_combo_->setObjectName("cloudSymbolCombo");
    symbol_combo_->addItems({"XAUUSD", "EURUSD", "GBPUSD", "USDJPY", "BTCUSD"});
    config_layout->addRow("Symbol:", symbol_combo_);

    timeframe_combo_ = new QComboBox(config_group);
    timeframe_combo_->setObjectName("cloudTimeframeCombo");
    timeframe_combo_->addItems({"M1", "M5", "M15", "M30", "H1", "H4", "D1"});
    config_layout->addRow("Timeframe:", timeframe_combo_);

    agents_spin_ = new QSpinBox(config_group);
    agents_spin_->setObjectName("cloudAgentsSpin");
    agents_spin_->setRange(1, 100);
    agents_spin_->setValue(10);
    agents_spin_->setSuffix(" agents");
    config_layout->addRow("Cloud Agents:", agents_spin_);

    generations_spin_ = new QSpinBox(config_group);
    generations_spin_->setObjectName("cloudGenerationsSpin");
    generations_spin_->setRange(10, 1000);
    generations_spin_->setValue(100);
    generations_spin_->setSuffix(" generations");
    config_layout->addRow("Generations:", generations_spin_);

    auto* btn_layout = new QHBoxLayout();
    start_btn_ = new QPushButton("START OPTIMIZATION", config_group);
    start_btn_->setObjectName("cloudStartBtn");
    start_btn_->setFixedHeight(34);
    connect(start_btn_, &QPushButton::clicked, this, &MT5FleetCloudPanel::on_start_clicked);
    btn_layout->addWidget(start_btn_);

    stop_btn_ = new QPushButton("STOP", config_group);
    stop_btn_->setObjectName("cloudStopBtn");
    stop_btn_->setFixedHeight(34);
    stop_btn_->setEnabled(false);
    connect(stop_btn_, &QPushButton::clicked, this, &MT5FleetCloudPanel::on_stop_clicked);
    btn_layout->addWidget(stop_btn_);

    config_layout->addRow(btn_layout);
    root->addWidget(config_group);

    // Status group
    auto* status_group = new QGroupBox("Optimization Status", this);
    status_group->setObjectName("cloudStatusGroup");
    auto* status_layout = new QVBoxLayout(status_group);
    status_layout->setSpacing(8);

    progress_bar_ = new QProgressBar(status_group);
    progress_bar_->setObjectName("cloudProgressBar");
    progress_bar_->setRange(0, 100);
    progress_bar_->setValue(0);
    status_layout->addWidget(progress_bar_);

    auto* status_info_layout = new QHBoxLayout();
    status_label_ = new QLabel("Idle", status_group);
    status_label_->setObjectName("cloudStatusLabel");
    status_info_layout->addWidget(status_label_);
    status_info_layout->addStretch();
    agents_label_ = new QLabel("Agents: 0/0", status_group);
    agents_label_->setObjectName("cloudAgentsLabel");
    status_info_layout->addWidget(agents_label_);
    status_layout->addLayout(status_info_layout);

    root->addWidget(status_group);

    // Results table
    auto* results_group = new QGroupBox("Optimization Results", this);
    results_group->setObjectName("cloudResultsGroup");
    auto* results_layout = new QVBoxLayout(results_group);

    results_table_ = new QTableWidget(0, 6, results_group);
    results_table_->setObjectName("cloudResultsTable");
    results_table_->setHorizontalHeaderLabels({
        "Pass", "Profit", "Drawdown", "Sharpe", "Win Rate", "Parameters"
    });
    results_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    results_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_table_->verticalHeader()->setVisible(false);
    results_layout->addWidget(results_table_);

    root->addWidget(results_group, 1);
}

void MT5FleetCloudPanel::apply_theme() {
    setStyleSheet(QString(
        "QGroupBox{color:%1;font-size:12px;font-weight:700;border:1px solid %2;margin-top:12px;padding-top:16px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 4px;}"
        "QComboBox{background:%3;color:%1;border:1px solid %2;padding:4px 8px;min-width:150px;}"
        "QSpinBox{background:%3;color:%1;border:1px solid %2;padding:4px 8px;min-width:100px;}"
        "QPushButton#cloudStartBtn{background:%4;color:#FFF;border:none;font-size:12px;font-weight:700;padding:0 20px;}"
        "QPushButton#cloudStartBtn:hover{background:#00B85C;}"
        "QPushButton#cloudStopBtn{background:%5;color:#FFF;border:none;font-size:12px;font-weight:700;padding:0 20px;}"
        "QPushButton#cloudStopBtn:hover{background:#FF3333;}"
        "QPushButton#cloudStopBtn:disabled{background:%6;color:%7;}"
        "QProgressBar{background:%3;border:1px solid %2;text-align:center;color:%1;}"
        "QProgressBar::chunk{background:%4;}"
        "QLabel#cloudStatusLabel{color:%1;font-size:11px;}"
        "QLabel#cloudAgentsLabel{color:%7;font-size:11px;}"
        "QTableWidget#cloudResultsTable{background:%8;color:%1;border:1px solid %2;}"
        "QTableWidget::item{padding:4px 8px;font-size:11px;}"
    ).arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(),
          ui::colors::POSITIVE(), ui::colors::NEGATIVE(), ui::colors::BG_HOVER(),
          ui::colors::TEXT_TERTIARY(), ui::colors::BG_BASE()));
}

void MT5FleetCloudPanel::refresh_optimizations() {
    HttpClient::instance().get("http://localhost:8150/mt5/optimizations", [this](Result<QJsonDocument> result) {
        if (result.is_err()) return;
        auto doc = result.value();
        auto optimizations = doc.object()["optimizations"].toArray();
        update_optimizations_table(optimizations);
    }, this);
}

void MT5FleetCloudPanel::update_optimizations_table(const QJsonArray& optimizations) {
    results_table_->setRowCount(optimizations.size());
    
    for (int i = 0; i < optimizations.size(); ++i) {
        auto opt = optimizations[i].toObject();
        
        results_table_->setItem(i, 0, new QTableWidgetItem(QString::number(opt["pass"].toInt())));
        
        double profit = opt["profit"].toDouble();
        auto* profit_item = new QTableWidgetItem(QString("$%1").arg(profit, 0, 'f', 2));
        profit_item->setForeground(profit >= 0 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE()));
        results_table_->setItem(i, 1, profit_item);
        
        double drawdown = opt["drawdown"].toDouble();
        auto* dd_item = new QTableWidgetItem(QString("%1%").arg(drawdown, 0, 'f', 2));
        dd_item->setForeground(drawdown < 20 ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE()));
        results_table_->setItem(i, 2, dd_item);
        
        results_table_->setItem(i, 3, new QTableWidgetItem(QString::number(opt["sharpe"].toDouble(), 'f', 2)));
        results_table_->setItem(i, 4, new QTableWidgetItem(QString("%1%").arg(opt["win_rate"].toDouble(), 0, 'f', 1)));
        results_table_->setItem(i, 5, new QTableWidgetItem(opt["parameters"].toString()));
    }
}

void MT5FleetCloudPanel::on_start_clicked() {
    QJsonObject config;
    config["ea_name"] = ea_combo_->currentText();
    config["symbol"] = symbol_combo_->currentText();
    config["timeframe"] = timeframe_combo_->currentText();
    config["agents"] = agents_spin_->value();
    config["generations"] = generations_spin_->value();

    HttpClient::instance().post(
        "http://localhost:8150/mt5/optimizations/start",
        config,
        [this](Result<QJsonDocument> result) {
            if (result.is_err()) {
                QMessageBox::warning(this, "Error", "Failed to start optimization");
                return;
            }
            auto doc = result.value();
            current_optimization_id_ = doc.object()["optimization_id"].toString();
            
            start_btn_->setEnabled(false);
            stop_btn_->setEnabled(true);
            status_label_->setText("Running...");
            progress_bar_->setValue(0);
            
            QMessageBox::information(this, "Started", "Optimization started!");
        }, this);
}

void MT5FleetCloudPanel::on_stop_clicked() {
    if (current_optimization_id_.isEmpty()) return;
    
    HttpClient::instance().post(
        QString("http://localhost:8150/mt5/optimizations/%1/stop").arg(current_optimization_id_),
        QJsonObject(),
        [this](Result<QJsonDocument> result) {
            if (result.is_err()) {
                QMessageBox::warning(this, "Error", "Failed to stop optimization");
                return;
            }
            
            start_btn_->setEnabled(true);
            stop_btn_->setEnabled(false);
            status_label_->setText("Stopped");
            
            QMessageBox::information(this, "Stopped", "Optimization stopped!");
        }, this);
}

} // namespace fincept::screens
