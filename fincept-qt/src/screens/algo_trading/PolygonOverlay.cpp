#include "screens/algo_trading/PolygonOverlay.h"
#include "screens/algo_trading/MT5FleetChartPanel.h"
#include "core/config/AppConfig.h"
#include "ui/theme/Theme.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace fincept::screens {

namespace {
QString polygon_ws_url() {
    QUrl url(fincept::AppConfig::instance().api_base_url());
    const QString scheme = url.scheme().toLower() == QStringLiteral("https") ? QStringLiteral("wss")
                                                                             : QStringLiteral("ws");
    url.setScheme(scheme);
    url.setPath(QStringLiteral("/ws/polygon"));
    return url.toString();
}
} // namespace

PolygonOverlay::PolygonOverlay(MT5FleetChartPanel* chart, QWidget* parent)
    : QWidget(parent), chart_(chart) {
    build_ui();
    ws_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(ws_, &QWebSocket::connected, this, [this]() {
        connected_ = true;
        conn_btn_->setText("Disconnect");
        status_lbl_->setText("Connected");
        // Auto-subscribe to default symbols
        QJsonObject msg;
        msg["action"] = "subscribe";
        msg["symbols"] = QJsonArray{"AAPL", "MSFT", "SPY"};
        msg["channel"] = "T";
        ws_->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    });
    connect(ws_, &QWebSocket::disconnected, this, [this]() {
        connected_ = false;
        conn_btn_->setText("Connect");
        status_lbl_->setText("Disconnected");
    });
    connect(ws_, &QWebSocket::textMessageReceived, this, &PolygonOverlay::on_ws_message);
}

PolygonOverlay::~PolygonOverlay() { ws_->close(); }

void PolygonOverlay::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(4,4,4,4); root->setSpacing(2);

    auto* hl = new QHBoxLayout();
    sym_edit_ = new QLineEdit("AAPL,MSFT,SPY", this);
    sym_edit_->setStyleSheet("background:#1a1a2e;color:#e5e5e5;border:1px solid #2a2a3e;padding:2px 6px;font-size:10px;");
    hl->addWidget(sym_edit_, 1);
    conn_btn_ = new QPushButton("Connect", this);
    conn_btn_->setObjectName("chartToolBtn"); conn_btn_->setFixedHeight(20);
    connect(conn_btn_, &QPushButton::clicked, this, &PolygonOverlay::on_connect);
    hl->addWidget(conn_btn_);
    status_lbl_ = new QLabel("Offline", this);
    status_lbl_->setStyleSheet("color:#808080;font-size:9px;");
    hl->addWidget(status_lbl_);
    root->addLayout(hl);

    auto makeRow = [&](const QString& label, QLabel*& val, const QString& color) {
        auto* r = new QHBoxLayout();
        auto* l = new QLabel(label, this); l->setStyleSheet("color:#808080;font-size:9px;font-weight:700;");
        r->addWidget(l);
        val = new QLabel("-", this); val->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;font-family:Consolas;").arg(color));
        r->addWidget(val, 1, Qt::AlignRight);
        root->addLayout(r);
    };
    makeRow("LAST",  last_lbl_,  "#22c55e");
    makeRow("BID",   bid_lbl_,  "#ef4444");
    makeRow("ASK",   ask_lbl_,  "#22c55e");
    makeRow("VOL",   vol_lbl_,  "#e5e5e5");
    makeRow("TIME",  ts_lbl_,   "#808080");
    setFixedWidth(200);
}

void PolygonOverlay::on_connect() {
    if (connected_) { ws_->close(); return; }
    status_lbl_->setText("Connecting...");
    ws_->open(QUrl(polygon_ws_url()));
}

void PolygonOverlay::on_ws_message(const QString& msg) {
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    if (!doc.isObject() && !doc.isArray()) return;

    auto process = [&](const QJsonObject& m) {
        QString ev = m["ev"].toString();
        if (ev == "T" || ev == "Q") {
            double price = m["p"].toDouble();
            double sz = m["s"].toDouble();
            qint64 ts = static_cast<qint64>(m["t"].toDouble());
            QString sym = m["sym"].toString();

            if (ev == "T") {
                last_lbl_->setText(QString("$%1").arg(price, 0, 'f', 2));
                vol_lbl_->setText(QString::number(sz, 'f', 0));
            } else {
                bid_lbl_->setText(QString("$%1").arg(m["bp"].toDouble(), 0, 'f', 2));
                ask_lbl_->setText(QString("$%1").arg(m["ap"].toDouble(), 0, 'f', 2));
            }
            ts_lbl_->setText(QDateTime::fromMSecsSinceEpoch(ts).toString("HH:mm:ss.zzz"));

            // Update chart price label
            if (chart_) chart_->set_price(price);
        }
    };

    if (doc.isArray()) {
        for (const auto& v : doc.array()) process(v.toObject());
    } else {
        process(doc.object());
    }
}

} // namespace fincept::screens
