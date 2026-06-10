#include "screens/backtesting/BacktestVisualizer.h"
#include "core/config/AppConfig.h"
#include "ui/theme/Theme.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>

namespace fincept::screens {

BacktestVisualizer::BacktestVisualizer(QWidget* parent) : QWidget(parent) {
    build_ui();
    ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(ws_, &QWebSocket::connected, this, &BacktestVisualizer::onConnected);
    connect(ws_, &QWebSocket::textMessageReceived, this, &BacktestVisualizer::onTextMessage);

    flush_timer_ = new QTimer(this);
    flush_timer_->setInterval(33);
    connect(flush_timer_, &QTimer::timeout, this, &BacktestVisualizer::onFlush);
}

BacktestVisualizer::~BacktestVisualizer() {
    stopBacktest();
}

void BacktestVisualizer::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);

    auto* bar = new QWidget(this); bar->setFixedHeight(30);
    auto* hl = new QHBoxLayout(bar); hl->setContentsMargins(8,0,8,0);
    start_btn_ = new QPushButton("Start Replay", this); start_btn_->setObjectName("chartToolBtn");
    connect(start_btn_, &QPushButton::clicked, this, [this]() {
        startBacktest("XAUUSD,SPY", "data/ticks", 100000);
    });
    hl->addWidget(start_btn_);
    stop_btn_ = new QPushButton("Stop", this); stop_btn_->setObjectName("chartToolBtn");
    stop_btn_->setEnabled(false);
    connect(stop_btn_, &QPushButton::clicked, this, &BacktestVisualizer::stopBacktest);
    hl->addWidget(stop_btn_);
    status_label_ = new QLabel("Ready", this); status_label_->setObjectName("chartTool");
    hl->addWidget(status_label_);
    equity_label_ = new QLabel("", this); equity_label_->setObjectName("chartPrice");
    hl->addWidget(equity_label_);
    zscore_label_ = new QLabel("", this); zscore_label_->setObjectName("chartTool");
    hl->addWidget(zscore_label_);
    root->addWidget(bar);

    chart_ = new QChart();
    chart_->setAnimationOptions(QChart::NoAnimation);
    chart_->legend()->hide();
    chart_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundVisible(true);

    equity_series_ = new QLineSeries();
    equity_series_->setPen(QPen(QColor(100, 255, 100), 1.5));
    chart_->addSeries(equity_series_);

    zscore_series_ = new QLineSeries();
    zscore_series_->setPen(QPen(QColor(255, 100, 100), 1, Qt::DashLine));
    chart_->addSeries(zscore_series_);

    axis_x_ = new QValueAxis();
    axis_x_->setLabelFormat("%.0f");
    axis_x_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    equity_series_->attachAxis(axis_x_);
    zscore_series_->attachAxis(axis_x_);

    axis_y_eq_ = new QValueAxis();
    axis_y_eq_->setLabelFormat("$%.0f");
    axis_y_eq_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_y_eq_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_y_eq_, Qt::AlignLeft);
    equity_series_->attachAxis(axis_y_eq_);

    axis_y_z_ = new QValueAxis();
    axis_y_z_->setLabelFormat("%.1f");
    axis_y_z_->setRange(-3, 3);
    axis_y_z_->setLabelsColor(QColor(255, 100, 100));
    axis_y_z_->setGridLineVisible(false);
    chart_->addAxis(axis_y_z_, Qt::AlignRight);
    zscore_series_->attachAxis(axis_y_z_);

    chart_view_ = new QChartView(chart_, this);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    root->addWidget(chart_view_, 1);
}

void BacktestVisualizer::startBacktest(const QString& symbols, const QString& dataDir, double cash) {
    if (running_) return;
    running_ = true;
    start_btn_->setEnabled(false);
    stop_btn_->setEnabled(true);
    status_label_->setText("Connecting...");
    equity_series_->clear();
    zscore_series_->clear();
    peak_eq_ = 0;
    buffer_.clear();
    has_data_ = false;

    {
        QUrl url(fincept::AppConfig::instance().api_base_url());
        url.setScheme(QStringLiteral("ws"));
        if (url.port() == 8155) url.setPort(8156);
        url.setPath(QStringLiteral("/backtest/ws/tick-replay"));
        ws_->open(url);
    }
}

void BacktestVisualizer::stopBacktest() {
    flush_timer_->stop();
    running_ = false;
    start_btn_->setEnabled(true);
    stop_btn_->setEnabled(false);
    ws_->close();
    status_label_->setText("Stopped");
}

void BacktestVisualizer::onConnected() {
    status_label_->setText("Connected, starting replay...");
    flush_timer_->start();

    QStringList syms = {"XAUUSD", "SPY"};
    QJsonObject cfg;
    cfg["symbols"] = QJsonArray::fromStringList(syms);
    cfg["data_dir"] = "data/ticks";
    cfg["cash"] = 100000;
    ws_->sendTextMessage(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
}

void BacktestVisualizer::onTextMessage(const QString& msg) {
    auto doc = QJsonDocument::fromJson(msg.toUtf8());
    if (!doc.isObject()) return;
    auto obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "PROGRESS") {
        ReplayPoint pt;
        pt.ts = static_cast<qint64>(obj["ts"].toDouble());
        pt.equity = obj["equity"].toDouble();
        pt.z_score = obj["z_score"].toDouble(0);
        QMutexLocker lock(&mutex_);
        buffer_.append(pt);
        latest_ = pt;
        has_data_ = true;
    } else if (type == "DONE" || type == "REPORT") {
        flush_timer_->stop();
        running_ = false;
        start_btn_->setEnabled(true);
        stop_btn_->setEnabled(false);
        status_label_->setText("Complete");
        if (type == "REPORT") {
            auto d = obj["data"].toObject();
            equity_label_->setText(QString("Final: $%1").arg(d["final_equity"].toDouble(), 0, 'f', 0));
        }
    } else if (type == "ERROR") {
        status_label_->setText("Error: " + obj["message"].toString());
        stopBacktest();
    }
}

void BacktestVisualizer::onFlush() {
    QMutexLocker lock(&mutex_);
    if (!has_data_ || buffer_.isEmpty()) return;

    if (buffer_.size() == 1) {
        equity_series_->append(latest_.ts, latest_.equity);
        zscore_series_->append(latest_.ts, latest_.z_score);
    } else {
        equity_series_->append(buffer_.first().ts, buffer_.first().equity);
        equity_series_->append(latest_.ts, latest_.equity);
        zscore_series_->append(buffer_.first().ts, buffer_.first().z_score);
        zscore_series_->append(latest_.ts, latest_.z_score);
    }
    peak_eq_ = qMax(peak_eq_, latest_.equity);
    axis_y_eq_->setRange(latest_.equity * 0.95, qMax(peak_eq_ * 1.05, latest_.equity * 1.05));
    axis_x_->setRange(latest_.ts - 60000, latest_.ts);
    equity_label_->setText(QString("$%1").arg(latest_.equity, 0, 'f', 0));
    zscore_label_->setText(QString("Z: %1").arg(latest_.z_score, 0, 'f', 2));
    buffer_.clear();
    has_data_ = false;
}

} // namespace fincept::screens
