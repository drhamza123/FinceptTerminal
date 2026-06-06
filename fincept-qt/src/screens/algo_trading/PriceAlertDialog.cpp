// PriceAlertDialog.cpp — Industrial-grade Price Alert management
#include "screens/algo_trading/PriceAlertDialog.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFrame>

namespace fincept::screens {

PriceAlertDialog::PriceAlertDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Price Alerts");
    setMinimumSize(700, 500);
    resize(800, 550);
    setObjectName("priceAlertDialog");

    all_symbols_ = {"XAUUSD","XAGUSD","EURUSD","GBPUSD","USDJPY","BTCUSD","ETHUSD",
                    "AAPL","MSFT","GOOGL","AMZN","TSLA","USOIL","USDCAD","AUDUSD"};

    build_ui();
    apply_theme();
    load_alerts();
}

PriceAlertDialog::~PriceAlertDialog() = default;

void PriceAlertDialog::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16,16,16,16);
    root->setSpacing(12);

    // Title
    auto* title = new QLabel("PRICE ALERTS");
    title->setObjectName("alertDialogTitle");
    root->addWidget(title);

    // Alert form group
    auto* form_group = new QGroupBox("Create Alert");
    form_group->setObjectName("alertFormGroup");
    auto* form = new QFormLayout(form_group);
    form->setSpacing(8);

    symbol_combo_ = new QComboBox(form_group);
    symbol_combo_->setObjectName("alertFormField");
    symbol_combo_->setEditable(true);
    symbol_combo_->addItems(all_symbols_);
    form->addRow("Symbol:", symbol_combo_);

    alert_type_combo_ = new QComboBox(form_group);
    alert_type_combo_->setObjectName("alertFormField");
    alert_type_combo_->addItems({"Price Above","Price Below","Cross Above","Cross Below","Out of Range"});
    connect(alert_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PriceAlertDialog::on_alert_type_changed);
    form->addRow("Type:", alert_type_combo_);

    auto* price_row = new QWidget(form_group);
    auto* price_hl = new QHBoxLayout(price_row);
    price_hl->setContentsMargins(0,0,0,0);
    price_hl->setSpacing(8);

    trigger_price_spin_ = new QDoubleSpinBox(price_row);
    trigger_price_spin_->setObjectName("alertFormField");
    trigger_price_spin_->setRange(0.00001, 9999999);
    trigger_price_spin_->setDecimals(5);
    trigger_price_spin_->setPrefix("$ ");
    trigger_price_spin_->setValue(0);
    price_hl->addWidget(trigger_price_spin_);

    range_label_ = new QLabel("to:", price_row);
    range_label_->setObjectName("alertRangeLabel");
    range_label_->hide();
    price_hl->addWidget(range_label_);

    range_high_spin_ = new QDoubleSpinBox(price_row);
    range_high_spin_->setObjectName("alertFormField");
    range_high_spin_->setRange(0.00001, 9999999);
    range_high_spin_->setDecimals(5);
    range_high_spin_->setPrefix("$ ");
    range_high_spin_->setValue(0);
    range_high_spin_->hide();
    price_hl->addWidget(range_high_spin_);

    price_hl->addStretch();
    form->addRow("Price:", price_row);

    channel_combo_ = new QComboBox(form_group);
    channel_combo_->setObjectName("alertFormField");
    channel_combo_->addItems({"Push Notification","Sound","Telegram","Email","Discord"});
    form->addRow("Notify via:", channel_combo_);

    note_input_ = new QLineEdit(form_group);
    note_input_->setObjectName("alertFormField");
    note_input_->setPlaceholderText("Optional note for this alert...");
    form->addRow("Note:", note_input_);

    add_btn_ = new QPushButton("+ ADD ALERT");
    add_btn_->setObjectName("alertAddBtn");
    add_btn_->setFixedHeight(32);
    connect(add_btn_, &QPushButton::clicked, this, &PriceAlertDialog::on_add_alert);
    form->addRow("", add_btn_);

    root->addWidget(form_group);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("alertSep");
    root->addWidget(sep);

    // Alert list
    auto* list_header = new QWidget(this);
    auto* lh = new QHBoxLayout(list_header);
    lh->setContentsMargins(0,0,0,0);
    auto* list_title = new QLabel("Active Alerts");
    list_title->setObjectName("alertListTitle");
    lh->addWidget(list_title);
    lh->addStretch();
    status_label_ = new QLabel("0 alerts");
    status_label_->setObjectName("alertListStatus");
    lh->addWidget(status_label_);
    root->addWidget(list_header);

    alert_table_ = new QTableWidget(0, 7, this);
    alert_table_->setObjectName("alertTable");
    alert_table_->setHorizontalHeaderLabels({"Symbol","Type","Price","Channel","Note","Status","Triggers"});
    alert_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    alert_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    alert_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    alert_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    alert_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    alert_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    alert_table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    alert_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    alert_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    alert_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    alert_table_->verticalHeader()->setVisible(false);
    alert_table_->setAlternatingRowColors(true);
    root->addWidget(alert_table_, 1);

    // Action buttons
    auto* actions = new QWidget(this);
    auto* al = new QHBoxLayout(actions);
    al->setContentsMargins(0,0,0,0);
    al->addStretch();
    toggle_btn_ = new QPushButton("Toggle");
    toggle_btn_->setObjectName("alertActionBtn");
    connect(toggle_btn_, &QPushButton::clicked, this, &PriceAlertDialog::on_toggle_alert);
    al->addWidget(toggle_btn_);
    delete_btn_ = new QPushButton("Delete");
    delete_btn_->setObjectName("alertActionBtn");
    connect(delete_btn_, &QPushButton::clicked, this, &PriceAlertDialog::on_delete_alert);
    al->addWidget(delete_btn_);
    auto* close_btn = new QPushButton("Close");
    close_btn->setObjectName("alertActionBtn");
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    al->addWidget(close_btn);
    root->addWidget(actions);
}

void PriceAlertDialog::apply_theme() {
    setStyleSheet(QString(
        "QDialog#priceAlertDialog{background:%1;}"
        "QLabel#alertDialogTitle{color:%2;font-size:14px;font-weight:700;letter-spacing:1.5px;padding-bottom:4px;}"
        "QGroupBox#alertFormGroup{background:%3;border:1px solid %4;border-radius:4px;padding:12px;margin-top:8px;font-weight:600;color:%5;}"
        "QComboBox#alertFormField,QDoubleSpinBox#alertFormField,QLineEdit#alertFormField{background:%6;color:%5;border:1px solid %4;padding:4px 8px;font-size:11px;border-radius:3px;min-height:20px;}"
        "QLabel#alertRangeLabel{color:%7;font-size:11px;}"
        "QPushButton#alertAddBtn{background:%2;color:%1;border:none;font-weight:700;font-size:11px;letter-spacing:1px;padding:0 20px;border-radius:3px;}"
        "QPushButton#alertAddBtn:hover{background:%8;}"
        "QFrame#alertSep{color:%4;max-height:1px;}"
        "QLabel#alertListTitle{color:%5;font-size:11px;font-weight:700;}"
        "QLabel#alertListStatus{color:%7;font-size:10px;}"
        "QTableWidget#alertTable{background:%6;color:%5;border:1px solid %4;font-size:11px;}"
        "QTableWidget#alertTable::item{padding:4px 8px;}"
        "QTableWidget#alertTable::item:selected{background:%2;color:%1;}"
        "QTableWidget#alertTable QHeaderView::section{background:%3;color:%7;border:none;border-right:1px solid %4;padding:4px 8px;font-size:9px;font-weight:700;}"
        "QPushButton#alertActionBtn{background:%3;color:%5;border:1px solid %4;padding:4px 14px;font-size:10px;font-weight:600;border-radius:3px;}"
        "QPushButton#alertActionBtn:hover{background:%6;}"
    ).arg(ui::colors::BG_BASE(), ui::colors::AMBER(), ui::colors::BG_SURFACE(),
          ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(),
          ui::colors::TEXT_TERTIARY(), "#D97706"));
}

void PriceAlertDialog::on_alert_type_changed(int idx) {
    bool is_range = (idx == 4);
    range_label_->setVisible(is_range);
    range_high_spin_->setVisible(is_range);
}

void PriceAlertDialog::on_add_alert() {
    PriceAlert alert;
    alert.id = QString();
    alert.symbol = symbol_combo_->currentText().toUpper().trimmed();
    if (alert.symbol.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a symbol");
        return;
    }

    int type_idx = alert_type_combo_->currentIndex();
    static const char* types[] = {"above","below","cross_above","cross_below","range"};
    alert.alert_type = types[type_idx];

    alert.trigger_price = trigger_price_spin_->value();
    if (alert.trigger_price <= 0) {
        QMessageBox::warning(this, "Error", "Please enter a valid trigger price");
        return;
    }

    if (type_idx == 4) {
        alert.range_high = range_high_spin_->value();
        if (alert.range_high <= alert.trigger_price) {
            QMessageBox::warning(this, "Error", "Range high must be greater than range low");
            return;
        }
    }

    alert.channel = channel_combo_->currentText().toLower().replace(" ", "_");
    alert.note = note_input_->text().trimmed();
    alert.enabled = true;
    alert.triggered_count = 0;
    alert.is_triggered = false;

    save_alert(alert);
    note_input_->clear();
    trigger_price_spin_->setValue(0);
    range_high_spin_->setValue(0);
    load_alerts();
    emit alertsChanged();
}

void PriceAlertDialog::on_delete_alert() {
    int row = alert_table_->currentRow();
    if (row < 0 || row >= alerts_.size()) return;

    auto reply = QMessageBox::question(this, "Delete Alert",
        "Are you sure you want to delete this alert?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    delete_alert(alerts_[row].id);
    load_alerts();
    emit alertsChanged();
}

void PriceAlertDialog::on_toggle_alert() {
    int row = alert_table_->currentRow();
    if (row < 0 || row >= alerts_.size()) return;

    auto& alert = alerts_[row];
    alert.enabled = !alert.enabled;
    alert.is_triggered = false;

    QJsonObject payload;
    payload["id"] = alert.id;
    payload["enabled"] = alert.enabled;

    HttpClient::instance().post(
        "http://localhost:8150/mt5/alert/toggle",
        payload,
        [this](Result<QJsonDocument>) {
            load_alerts();
            emit alertsChanged();
        }, this);
}

void PriceAlertDialog::load_alerts() {
    HttpClient::instance().get(
        "http://localhost:8150/mt5/alerts",
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto obj = r.value().object();
            auto data = obj["data"].toArray();

            alerts_.clear();
            for (auto v : data) {
                auto a = v.toObject();
                PriceAlert alert;
                alert.id = a["id"].toString();
                alert.symbol = a["symbol"].toString();
                alert.alert_type = a["alert_type"].toString();
                alert.trigger_price = a["trigger_price"].toDouble();
                alert.range_high = a["range_high"].toDouble();
                alert.enabled = a["enabled"].toBool(true);
                alert.channel = a["channel"].toString();
                alert.note = a["note"].toString();
                alert.triggered_count = a["triggered_count"].toInt();
                alert.is_triggered = a["is_triggered"].toBool();
                alerts_.append(alert);
            }
            refresh_alerts();
        }, this);
}

void PriceAlertDialog::save_alert(const PriceAlert& alert) {
    QJsonObject payload;
    payload["symbol"] = alert.symbol;
    payload["alert_type"] = alert.alert_type;
    payload["trigger_price"] = alert.trigger_price;
    if (alert.alert_type == "range") payload["range_high"] = alert.range_high;
    payload["channel"] = alert.channel;
    payload["note"] = alert.note;
    payload["enabled"] = alert.enabled;
    if (!alert.id.isEmpty()) payload["id"] = alert.id;

    HttpClient::instance().post(
        "http://localhost:8150/mt5/alert/save",
        payload,
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                QMessageBox::warning(this, "Error", "Failed to save alert");
            }
        }, this);
}

void PriceAlertDialog::delete_alert(const QString& id) {
    QJsonObject payload;
    payload["id"] = id;

    HttpClient::instance().post(
        "http://localhost:8150/mt5/alert/delete",
        payload,
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) {
                QMessageBox::warning(this, "Error", "Failed to delete alert");
            }
        }, this);
}

void PriceAlertDialog::refresh_alerts() {
    alert_table_->setRowCount(alerts_.size());
    status_label_->setText(QString("%1 alerts (%2 enabled)")
        .arg(alerts_.size())
        .arg(std::count_if(alerts_.begin(), alerts_.end(), [](const PriceAlert& a){ return a.enabled; })));

    for (int i = 0; i < alerts_.size(); ++i) {
        const auto& a = alerts_[i];
        alert_table_->setItem(i, 0, new QTableWidgetItem(a.symbol));
        QString type_display = a.alert_type;
        type_display.replace("_", " ");
        type_display[0] = type_display[0].toUpper();
        auto* type_item = new QTableWidgetItem(type_display);
        if (a.alert_type.contains("above")) type_item->setForeground(QColor(ui::colors::POSITIVE()));
        else if (a.alert_type.contains("below")) type_item->setForeground(QColor(ui::colors::NEGATIVE()));
        else type_item->setForeground(QColor(ui::colors::AMBER()));
        alert_table_->setItem(i, 1, type_item);

        QString price_text = QString("$%1").arg(a.trigger_price, 0, 'f', 2);
        if (a.alert_type == "range") {
            price_text += QString(" - $%1").arg(a.range_high, 0, 'f', 2);
        }
        alert_table_->setItem(i, 2, new QTableWidgetItem(price_text));
        alert_table_->setItem(i, 3, new QTableWidgetItem(a.channel));
        alert_table_->setItem(i, 4, new QTableWidgetItem(a.note));

        auto* status_item = new QTableWidgetItem(a.enabled ? "ACTIVE" : "PAUSED");
        status_item->setForeground(a.enabled ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::TEXT_TERTIARY()));
        status_item->setFont(QFont("", 9, QFont::Bold));
        alert_table_->setItem(i, 5, status_item);

        alert_table_->setItem(i, 6, new QTableWidgetItem(QString::number(a.triggered_count)));
        alert_table_->setRowHeight(i, 28);
    }
}

} // namespace fincept::screens
