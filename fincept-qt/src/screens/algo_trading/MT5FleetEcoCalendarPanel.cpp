// MT5FleetEcoCalendarPanel.cpp — Industrial-grade Economic Calendar
#include "screens/algo_trading/MT5FleetEcoCalendarPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QShowEvent>

namespace fincept::screens {

static const int POLL_SECS = 60;

MT5FleetEcoCalendarPanel::MT5FleetEcoCalendarPanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_theme();
}

MT5FleetEcoCalendarPanel::~MT5FleetEcoCalendarPanel() = default;

void MT5FleetEcoCalendarPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("ecoCalHeader");
    header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10,0,10,0);
    hl->setSpacing(8);

    auto* title = new QLabel("ECONOMIC CALENDAR", header);
    title->setObjectName("ecoCalTitle");
    hl->addWidget(title);
    hl->addStretch();

    count_label_ = new QLabel("0 events", header);
    count_label_->setObjectName("ecoCalCount");
    hl->addWidget(count_label_);

    root->addWidget(header);

    // Filter bar
    auto* filter_bar = new QWidget(this);
    filter_bar->setObjectName("ecoCalFilter");
    filter_bar->setFixedHeight(32);
    auto* fl = new QHBoxLayout(filter_bar);
    fl->setContentsMargins(8,2,8,2);
    fl->setSpacing(6);

    auto* imp_lbl = new QLabel("Impact:", filter_bar);
    imp_lbl->setObjectName("ecoCalFilterLabel");
    fl->addWidget(imp_lbl);

    impact_filter_ = new QComboBox(filter_bar);
    impact_filter_->setObjectName("ecoCalFilterCombo");
    impact_filter_->addItems({"All","High","Medium","Low"});
    connect(impact_filter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetEcoCalendarPanel::on_filter_change);
    fl->addWidget(impact_filter_);

    auto* cnt_lbl = new QLabel("Country:", filter_bar);
    cnt_lbl->setObjectName("ecoCalFilterLabel");
    fl->addWidget(cnt_lbl);

    country_filter_ = new QComboBox(filter_bar);
    country_filter_->setObjectName("ecoCalFilterCombo");
    country_filter_->addItems({"All","US","EU","UK","JP","CN","AU","CH"});
    connect(country_filter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetEcoCalendarPanel::on_filter_change);
    fl->addWidget(country_filter_);

    auto* cat_lbl = new QLabel("Category:", filter_bar);
    cat_lbl->setObjectName("ecoCalFilterLabel");
    fl->addWidget(cat_lbl);

    category_filter_ = new QComboBox(filter_bar);
    category_filter_->setObjectName("ecoCalFilterCombo");
    category_filter_->addItems({"All","Employment","Inflation","Central Bank","GDP","Housing","Manufacturing","Trade","Consumer Confidence","Services"});
    connect(category_filter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetEcoCalendarPanel::on_filter_change);
    fl->addWidget(category_filter_);

    fl->addStretch();
    root->addWidget(filter_bar);

    // Table
    table_ = new QTableWidget(0, 8, this);
    table_->setObjectName("ecoCalTable");
    table_->setHorizontalHeaderLabels({"Time","Country","Event","Category","Impact","Actual","Forecast","Previous"});
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);
    root->addWidget(table_, 1);

    // Timer
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &MT5FleetEcoCalendarPanel::refresh_events);
}

void MT5FleetEcoCalendarPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#ecoCalHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#ecoCalTitle{color:%3;font-size:11px;font-weight:700;letter-spacing:1.5px;background:transparent;}"
        "QLabel#ecoCalCount{color:%4;font-size:10px;background:transparent;}"
        "QWidget#ecoCalFilter{background:%1;border-bottom:1px solid %2;}"
        "QLabel#ecoCalFilterLabel{color:%4;font-size:10px;font-weight:600;background:transparent;}"
        "QComboBox#ecoCalFilterCombo{background:%5;color:%6;border:1px solid %2;padding:2px 6px;font-size:10px;min-width:100px;}"
        "QTableWidget#ecoCalTable{background:%7;color:%6;border:none;font-size:11px;}"
        "QTableWidget#ecoCalTable::item{padding:6px 8px;border-bottom:1px solid %8;}"
        "QTableWidget#ecoCalTable::item:selected{background:%3;color:%7;}"
        "QTableWidget#ecoCalTable QHeaderView::section{background:%1;color:%4;border:none;border-right:1px solid %2;padding:6px 8px;font-size:9px;font-weight:700;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
          ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
}

void MT5FleetEcoCalendarPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (!is_active_) {
        is_active_ = true;
        refresh_events();
        timer_->start(POLL_SECS * 1000);
    }
}

void MT5FleetEcoCalendarPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    is_active_ = false;
    timer_->stop();
}

void MT5FleetEcoCalendarPanel::refresh_events() {
    HttpClient::instance().get(
        "/macro/upcoming-events?limit=60",
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto obj = r.value().object();
            auto data = obj["data"].toObject();
            auto events_arr = data["events"].toArray();

            all_events_.clear();
            for (auto v : events_arr) {
                auto e = v.toObject();
                EcoEvent ev;
                ev.event_id = e["event_id"].toString();
                ev.title = e["title"].toString();
                ev.date = e["date"].toString();
                ev.time = e["time"].toString();
                ev.country = e["country"].toString();
                ev.category = e["category"].toString();
                ev.importance = e["importance"].toString();
                ev.previous = e["previous"].toString();
                ev.forecast = e["forecast"].toString();
                ev.actual = e["actual"].toString();
                if (ev.actual == "—" || ev.actual.isEmpty()) ev.actual = "";
                if (ev.forecast == "—" || ev.forecast.isEmpty()) ev.forecast = "—";
                if (ev.previous == "—" || ev.previous.isEmpty()) ev.previous = "—";
                all_events_.append(ev);
            }

            count_label_->setText(QString("%1 events").arg(all_events_.size()));
            on_filter_change();
        }, this);
}

void MT5FleetEcoCalendarPanel::on_filter_change() {
    QString imp_filter = impact_filter_->currentText();
    QString cnt_filter = country_filter_->currentText();
    QString cat_filter = category_filter_->currentText();

    QVector<EcoEvent> filtered;
    for (const auto& ev : all_events_) {
        if (imp_filter != "All" && ev.importance.compare(imp_filter, Qt::CaseInsensitive) != 0) continue;
        if (cnt_filter != "All" && ev.country != cnt_filter) continue;
        if (cat_filter != "All" && ev.category != cat_filter) continue;
        filtered.append(ev);
    }

    count_label_->setText(QString("%1 / %2 events").arg(filtered.size()).arg(all_events_.size()));
    populate_table(filtered);
}

void MT5FleetEcoCalendarPanel::populate_table(const QVector<EcoEvent>& events) {
    table_->setRowCount(events.size());
    for (int i = 0; i < events.size(); ++i) {
        add_event_row(i, events[i]);
    }
}

void MT5FleetEcoCalendarPanel::add_event_row(int row, const EcoEvent& event) {
    // Time
    auto* time_item = new QTableWidgetItem(event.date + "\n" + event.time);
    time_item->setFont(QFont("Consolas", 9));
    table_->setItem(row, 0, time_item);

    // Country with flag
    auto* cnt_item = new QTableWidgetItem(country_flag(event.country) + " " + event.country);
    cnt_item->setForeground(imp_country_color(event.country));
    cnt_item->setFont(QFont("", 10, QFont::Bold));
    table_->setItem(row, 1, cnt_item);

    // Event title
    auto* title_item = new QTableWidgetItem(event.title);
    title_item->setFont(QFont("", 10, QFont::Medium));
    table_->setItem(row, 2, title_item);

    // Category
    auto* cat_item = new QTableWidgetItem(event.category);
    table_->setItem(row, 3, cat_item);

    // Impact with color
    auto* imp_item = new QTableWidgetItem(event.importance.toUpper());
    QColor ic = imp_color(event.importance);
    imp_item->setForeground(ic);
    imp_item->setFont(QFont("", 10, QFont::Bold));
    if (event.importance == "high") imp_item->setBackground(QColor(255, 50, 50, 20));
    table_->setItem(row, 4, imp_item);

    // Actual (green if better than forecast, red if worse)
    auto* act_item = new QTableWidgetItem(event.actual.isEmpty() ? "—" : event.actual);
    if (!event.actual.isEmpty() && !event.forecast.isEmpty() && event.forecast != "—") {
        bool act_ok, fcast_ok;
        double act_val = event.actual.toDouble(&act_ok);
        double fcast_val = event.forecast.toDouble(&fcast_ok);
        if (act_ok && fcast_ok) {
            if (act_val >= fcast_val) act_item->setForeground(QColor(ui::colors::POSITIVE()));
            else act_item->setForeground(QColor(ui::colors::NEGATIVE()));
        }
    }
    act_item->setFont(QFont("Consolas", 10, QFont::Bold));
    table_->setItem(row, 5, act_item);

    // Forecast
    auto* fcast_item = new QTableWidgetItem(event.forecast);
    fcast_item->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
    table_->setItem(row, 6, fcast_item);

    // Previous
    auto* prev_item = new QTableWidgetItem(event.previous);
    prev_item->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
    table_->setItem(row, 7, prev_item);

    // Row height
    table_->setRowHeight(row, 32);
}

QColor MT5FleetEcoCalendarPanel::imp_country_color(const QString& country) const {
    if (country == "US") return QColor("#3B82F6");
    if (country == "EU") return QColor("#F59E0B");
    if (country == "UK") return QColor("#EF4444");
    if (country == "JP") return QColor("#EC4899");
    if (country == "CN") return QColor("#DC2626");
    if (country == "AU") return QColor("#10B981");
    if (country == "CH") return QColor("#8B5CF6");
    return QColor(ui::colors::TEXT_PRIMARY());
}

QColor MT5FleetEcoCalendarPanel::imp_color(const QString& importance) const {
    if (importance == "high") return QColor("#EF4444");
    if (importance == "medium") return QColor("#F59E0B");
    if (importance == "low") return QColor("#10B981");
    return QColor(ui::colors::TEXT_TERTIARY());
}

QString MT5FleetEcoCalendarPanel::country_flag(const QString& country) const {
    if (country == "US") return QString::fromUtf8("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"); // 🇺🇸
    if (country == "EU") return QString::fromUtf8("\xF0\x9F\x87\xAA\xF0\x9F\x87\xBA"); // 🇪🇺
    if (country == "UK") return QString::fromUtf8("\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7"); // 🇬🇧
    if (country == "JP") return QString::fromUtf8("\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"); // 🇯🇵
    if (country == "CN") return QString::fromUtf8("\xF0\x9F\x87\xA8\xF0\x9F\x87\xB3"); // 🇨🇳
    if (country == "AU") return QString::fromUtf8("\xF0\x9F\x87\xA6\xF0\x9F\x87\xBA"); // 🇦🇺
    if (country == "CH") return QString::fromUtf8("\xF0\x9F\x87\xA8\xF0\x9F\x87\xAD"); // 🇨🇭
    if (country == "NZ") return QString::fromUtf8("\xF0\x9F\x87\xB3\xF0\x9F\x87\xBF"); // 🇳🇿
    if (country == "CA") return QString::fromUtf8("\xF0\x9F\x87\xA8\xF0\x9F\x87\xA6"); // 🇨🇦
    return "";
}

} // namespace fincept::screens
