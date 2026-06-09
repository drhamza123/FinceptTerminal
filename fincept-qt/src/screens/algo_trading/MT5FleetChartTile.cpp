// MT5FleetChartTile.cpp — Independent chart tile with candlestick/bar/line/area
#include "screens/algo_trading/MT5FleetChartTile.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QAreaSeries>
#include <QPen>
#include <QBrush>


namespace fincept::screens {

static const int MAX_CANDLES = 150;

MT5FleetChartTile::MT5FleetChartTile(int index, QWidget* parent)
    : QWidget(parent), index_(index) {
    build_ui();
    apply_theme();
}

MT5FleetChartTile::~MT5FleetChartTile() = default;

void MT5FleetChartTile::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(1,1,1,1);
    root->setSpacing(0);

    // Mini header
    auto* header = new QWidget(this);
    header->setObjectName("tileHeader");
    header->setFixedHeight(24);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(6,0,6,0);
    hl->setSpacing(4);

    title_label_ = new QLabel(QString("[%1] %2 H1").arg(index_+1).arg(symbol_), header);
    title_label_->setObjectName("tileTitle");
    hl->addWidget(title_label_);
    hl->addStretch();

    style_combo_ = new QComboBox(header);
    style_combo_->setObjectName("tileStyleCombo");
    style_combo_->addItems({"Candle","Bar","Line","Area"});
    style_combo_->setFixedWidth(70);
    connect(style_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        setChartStyle(static_cast<ChartStyle>(idx));
    });
    hl->addWidget(style_combo_);

    sync_btn_ = new QPushButton("🔗", header);
    sync_btn_->setObjectName("tileSyncBtn");
    sync_btn_->setFixedSize(22, 22);
    sync_btn_->setCheckable(true);
    sync_btn_->setToolTip("Sync timeframe to other tiles");
    hl->addWidget(sync_btn_);

    root->addWidget(header);

    // Chart
    chart_ = new QChart();
    chart_->setAnimationOptions(QChart::SeriesAnimations);
    chart_->legend()->hide();
    chart_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(2,2,2,2));

    chart_view_ = new QChartView(chart_, this);
    chart_view_->setObjectName("tileChartView");
    chart_view_->setRenderHint(QPainter::Antialiasing);
    root->addWidget(chart_view_, 1);
}

void MT5FleetChartTile::apply_theme() {
    setStyleSheet(QString(
        "QWidget#tileHeader{background:%1;border:1px solid %2;border-bottom:none;}"
        "QLabel#tileTitle{color:%3;font-size:10px;font-weight:600;background:transparent;}"
        "QComboBox#tileStyleCombo{background:%4;color:%3;border:1px solid %2;font-size:9px;padding:1px 4px;}"
        "QPushButton#tileSyncBtn{background:%4;color:%3;border:1px solid %2;font-size:10px;}"
        "QPushButton#tileSyncBtn:checked{background:%5;color:%6;}"
        "QChartView#tileChartView{background:%6;border:1px solid %2;border-top:none;}"
    ).arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_SURFACE(), ui::colors::AMBER(), ui::colors::BG_BASE()));
}

void MT5FleetChartTile::setSymbol(const QString& symbol) {
    symbol_ = symbol;
    title_label_->setText(QString("[%1] %2 %3").arg(index_+1).arg(symbol_).arg(timeframe_));
    loadData();
}

void MT5FleetChartTile::setTimeframe(const QString& tf) {
    timeframe_ = tf;
    title_label_->setText(QString("[%1] %2 %3").arg(index_+1).arg(symbol_).arg(timeframe_));
    loadData();
}

void MT5FleetChartTile::setChartStyle(ChartStyle style) {
    style_ = style;
    loadData();
}

void MT5FleetChartTile::clearData() {
    data_.clear();
    clear_series();
}

void MT5FleetChartTile::clear_series() {
    chart_->removeAllSeries();
    if (axis_x_) { chart_->removeAxis(axis_x_); axis_x_ = nullptr; }
    if (axis_y_) { chart_->removeAxis(axis_y_); axis_y_ = nullptr; }
}

void MT5FleetChartTile::loadData() {
    HttpClient::instance().get(
        QString("/mt5/market/ohlc?symbol=%1&timeframe=%2&count=%3")
            .arg(symbol_, timeframe_).arg(MAX_CANDLES),
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            if (arr.isEmpty()) return;

            data_.clear();
            for (auto v : arr) {
                auto obj = v.toObject();
                OhlcPoint p;
                p.time = obj["time"].toDouble() * 1000;
                p.open = obj["open"].toDouble();
                p.high = obj["high"].toDouble();
                p.low = obj["low"].toDouble();
                p.close = obj["close"].toDouble();
                p.volume = obj["tick_volume"].toDouble();
                data_.append(p);
            }

            clear_series();
            switch (style_) {
                case ChartStyle::Candlestick: render_candlestick(); break;
                case ChartStyle::Bar: render_bar(); break;
                case ChartStyle::Line: render_line(); break;
                case ChartStyle::Area: render_area(); break;
            }
        }, this);
}

void MT5FleetChartTile::render_candlestick() {
    if (data_.isEmpty()) return;

    auto* candlestick = new QCandlestickSeries();
    candlestick->setName(symbol_);
    candlestick->setIncreasingColor(QColor(ui::colors::POSITIVE()));
    candlestick->setDecreasingColor(QColor(ui::colors::NEGATIVE()));
    candlestick->setBodyWidth(0.8);
    candlestick->setPen(QPen(QColor(180,180,180), 1));

    qreal min_p = 1e9, max_p = 0;
    qreal min_t = data_.first().time;
    qreal max_t = data_.last().time;

    for (const auto& p : data_) {
        auto* set = new QCandlestickSet(p.open, p.high, p.low, p.close, p.time);
        candlestick->append(set);
        min_p = qMin(min_p, p.low);
        max_p = qMax(max_p, p.high);
    }

    chart_->addSeries(candlestick);

    // Axes
    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("dd HH:mm");
    axis_x_->setTickCount(5);
    axis_x_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(min_t)),
                      QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(max_t)));
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    candlestick->attachAxis(axis_x_);

    axis_y_ = new QValueAxis();
    double pad = (max_p - min_p) * 0.08;
    axis_y_->setRange(min_p - pad, max_p + pad);
    axis_y_->setLabelFormat("%.2f");
    axis_y_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_y_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    candlestick->attachAxis(axis_y_);
}

void MT5FleetChartTile::render_bar() {
    // OHLC Bar chart using QLineSeries (open-high-low-close as thin lines)
    if (data_.isEmpty()) return;

    qreal min_p = 1e9, max_p = 0;
    qreal min_t = data_.first().time;
    qreal max_t = data_.last().time;

    for (const auto& p : data_) {
        // Each bar is a vertical line from low to high with ticks at open and close
        auto* bar_line = new QLineSeries();
        bar_line->setPen(QPen(p.close >= p.open ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE()), 1));

        // High-low line (need 2 points)
        bar_line->append(p.time, p.low);
        bar_line->append(p.time, p.high);

        // Open tick (small horizontal line to the left)
        auto* open_tick = new QLineSeries();
        open_tick->setPen(QPen(QColor(180,180,180), 1));
        open_tick->append(p.time - 1800000, p.open);  // 30min left offset
        open_tick->append(p.time, p.open);

        // Close tick (small horizontal line to the right)
        auto* close_tick = new QLineSeries();
        close_tick->setPen(QPen(p.close >= p.open ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE()), 1));
        close_tick->append(p.time, p.close);
        close_tick->append(p.time + 1800000, p.close);  // 30min right offset

        chart_->addSeries(bar_line);
        chart_->addSeries(open_tick);
        chart_->addSeries(close_tick);

        min_p = qMin(min_p, p.low);
        max_p = qMax(max_p, p.high);
    }

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("dd HH:mm");
    axis_x_->setTickCount(5);
    axis_x_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(min_t)),
                      QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(max_t)));
    chart_->addAxis(axis_x_, Qt::AlignBottom);

    axis_y_ = new QValueAxis();
    double pad = (max_p - min_p) * 0.08;
    axis_y_->setRange(min_p - pad, max_p + pad);
    axis_y_->setLabelFormat("%.2f");
    axis_y_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_y_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_y_, Qt::AlignLeft);

    // Attach all series to axes
    for (auto* s : chart_->series()) {
        s->attachAxis(axis_x_);
        s->attachAxis(axis_y_);
    }
}

void MT5FleetChartTile::render_line() {
    if (data_.isEmpty()) return;

    auto* line = new QLineSeries();
    line->setPen(QPen(QColor(100, 180, 255), 1.5));
    line->setName(symbol_);

    qreal min_p = 1e9, max_p = 0;
    qreal min_t = data_.first().time;
    qreal max_t = data_.last().time;

    for (const auto& p : data_) {
        line->append(p.time, p.close);
        min_p = qMin(min_p, p.close);
        max_p = qMax(max_p, p.close);
    }

    chart_->addSeries(line);

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("dd HH:mm");
    axis_x_->setTickCount(5);
    axis_x_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(min_t)),
                      QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(max_t)));
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    line->attachAxis(axis_x_);

    axis_y_ = new QValueAxis();
    double pad = (max_p - min_p) * 0.08;
    axis_y_->setRange(min_p - pad, max_p + pad);
    axis_y_->setLabelFormat("%.2f");
    axis_y_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_y_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    line->attachAxis(axis_y_);
}

void MT5FleetChartTile::render_area() {
    if (data_.isEmpty()) return;

    auto* line = new QLineSeries();
    auto* upper = new QLineSeries();

    qreal min_p = 1e9, max_p = 0;
    qreal min_t = data_.first().time;
    qreal max_t = data_.last().time;

    for (const auto& p : data_) {
        line->append(p.time, p.close);
        upper->append(p.time, p.close);
        min_p = qMin(min_p, p.close);
        max_p = qMax(max_p, p.close);
    }

    auto* area = new QAreaSeries(line, upper);
    QColor area_color(100, 180, 255, 60);
    area->setColor(area_color);
    area->setBorderColor(QColor(100, 180, 255));
    area->setName(symbol_);

    chart_->addSeries(area);

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("dd HH:mm");
    axis_x_->setTickCount(5);
    axis_x_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(min_t)),
                      QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(max_t)));
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    area->attachAxis(axis_x_);

    axis_y_ = new QValueAxis();
    double pad = (max_p - min_p) * 0.08;
    axis_y_->setRange(min_p - pad, max_p + pad);
    axis_y_->setLabelFormat("%.2f");
    axis_y_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_y_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    area->attachAxis(axis_y_);
}

} // namespace fincept::screens
