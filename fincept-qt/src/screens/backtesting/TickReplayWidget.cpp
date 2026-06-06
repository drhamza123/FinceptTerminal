#include "screens/backtesting/TickReplayWidget.h"
#include "ui/theme/Theme.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

namespace fincept::screens {

TickReplayWidget::TickReplayWidget(QWidget* parent) : QWidget(parent) {
    build_ui();
    ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(ws_, &QWebSocket::connected, this, [this]() {
        connected_ = true;
        connect_btn_->setText("Disconnect");
        start_btn_->setEnabled(true);
        status_label_->setText("Connected");
    });
    connect(ws_, &QWebSocket::disconnected, this, &TickReplayWidget::on_disconnected);
    connect(ws_, &QWebSocket::textMessageReceived, this, &TickReplayWidget::on_text_message);
}

TickReplayWidget::~TickReplayWidget() {
    if (ws_) ws_->close();
}

void TickReplayWidget::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0);

    auto* bar = new QWidget(this); bar->setFixedHeight(32);
    auto* hl = new QHBoxLayout(bar); hl->setContentsMargins(8,0,8,0);
    hl->addWidget(new QLabel("Data:", this));
    data_dir_edit_ = new QLineEdit("data/ticks", this); data_dir_edit_->setFixedWidth(120);
    hl->addWidget(data_dir_edit_);
    hl->addWidget(new QLabel("Symbols:", this));
    symbols_edit_ = new QLineEdit("XAUUSD,SPY", this); symbols_edit_->setFixedWidth(140);
    hl->addWidget(symbols_edit_);
    hl->addWidget(new QLabel("Cash:", this));
    cash_spin_ = new QDoubleSpinBox(this); cash_spin_->setRange(1000, 1e9);
    cash_spin_->setValue(100000); cash_spin_->setFixedWidth(90);
    hl->addWidget(cash_spin_);
    connect_btn_ = new QPushButton("Connect", this); connect_btn_->setObjectName("chartToolBtn");
    connect(connect_btn_, &QPushButton::clicked, this, [this]() {
        if (connected_) { ws_->close(); return; }
        status_label_->setText("Connecting...");
        ws_->open(QUrl("ws://localhost:8150/backtest/ws/tick-replay"));
    });
    hl->addWidget(connect_btn_);
    start_btn_ = new QPushButton("Start Replay", this); start_btn_->setObjectName("chartToolBtn");
    start_btn_->setEnabled(false);
    connect(start_btn_, &QPushButton::clicked, this, &TickReplayWidget::on_start_replay);
    hl->addWidget(start_btn_);
    status_label_ = new QLabel("Disconnected", this); status_label_->setObjectName("chartTool");
    hl->addWidget(status_label_);
    equity_label_ = new QLabel("", this); equity_label_->setObjectName("chartPrice");
    hl->addWidget(equity_label_);
    trade_count_label_ = new QLabel("", this); trade_count_label_->setObjectName("chartTool");
    hl->addWidget(trade_count_label_);
    root->addWidget(bar);

    chart_ = new QChart();
    chart_->setAnimationOptions(QChart::SeriesAnimations);
    chart_->legend()->hide();
    chart_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundVisible(true);

    equity_series_ = new QLineSeries();
    equity_series_->setPen(QPen(QColor(100, 200, 255), 2));
    chart_->addSeries(equity_series_);

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("HH:mm:ss");
    axis_x_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    equity_series_->attachAxis(axis_x_);

    axis_y_ = new QValueAxis();
    axis_y_->setLabelFormat("$%.0f");
    axis_y_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_y_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    axis_y_->setTitleText("Equity");
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    equity_series_->attachAxis(axis_y_);

    chart_view_ = new QChartView(chart_, this);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    root->addWidget(chart_view_, 1);
}

void TickReplayWidget::on_disconnected() {
    connected_ = false;
    running_ = false;
    connect_btn_->setText("Connect");
    start_btn_->setEnabled(false);
    status_label_->setText("Disconnected");
}

void TickReplayWidget::on_text_message(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    auto obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "PROGRESS" || type == "DONE") {
        double eq = obj["equity"].toDouble();
        double ts = obj["ts"].toDouble();
        int trades = obj["trades"].toInt();

        equity_series_->append(ts, eq);
        peak_equity_ = qMax(peak_equity_, eq);
        axis_y_->setRange(eq * 0.95, peak_equity_ * 1.05);

        double range = qMax(60000.0, ts - equity_series_->points().first().x());
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ts - range)),
                          QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ts)));
        equity_label_->setText(QString("Equity: $%1").arg(eq, 0, 'f', 0));
        trade_count_label_->setText(QString("Trades: %1").arg(trades));

        if (type == "DONE") {
            running_ = false; start_btn_->setEnabled(true);
            status_label_->setText("Replay complete");
        }
    } else if (type == "REPORT") {
        auto d = obj["data"].toObject();
        equity_label_->setText(QString("Final: $%1 | Trades: %2")
            .arg(d["final_equity"].toDouble(), 0, 'f', 0).arg(d["trades"].toInt()));
        status_label_->setText("Complete");
    } else if (type == "ERROR") {
        status_label_->setText("Error: " + obj["message"].toString());
    }
}

void TickReplayWidget::on_start_replay() {
    if (running_ || !connected_) return;
    running_ = true; start_btn_->setEnabled(false);
    status_label_->setText("Replaying...");
    equity_series_->clear(); peak_equity_ = 0;

    QJsonObject cfg;
    cfg["symbols"] = QJsonArray::fromStringList(symbols_edit_->text().split(","));
    cfg["data_dir"] = data_dir_edit_->text();
    cfg["cash"] = cash_spin_->value();
    ws_->sendTextMessage(QJsonDocument(cfg).toJson(QJsonDocument::Compact));
}

} // namespace fincept::screens
