#include "ui/charts/IndicatorPanel.h"

#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QDateTimeAxis>
#include <QGraphicsLayout>
#include <QValueAxis>
#include <QPen>

namespace fincept::ui {

IndicatorPanel::IndicatorPanel(Type type, int height, QWidget* parent)
    : QChartView(parent), type_(type) {
    setRenderHint(QPainter::Antialiasing);
    setFixedHeight(height);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet("background: transparent;");

    chart_ = new QChart();
    chart_->setBackgroundBrush(QBrush(QColor("#131722")));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor("#1e222d")));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->legend()->hide();
    chart_->layout()->setContentsMargins(0, 0, 0, 0);
    setChart(chart_);

    x_axis_ = new QDateTimeAxis();
    x_axis_->setFormat("HH:mm");
    x_axis_->setLabelsColor(QColor("#787b86"));
    x_axis_->setGridLineColor(QColor("#2a2e39"));
    x_axis_->setLinePen(QPen(Qt::NoPen));
    x_axis_->setTickCount(5);
    chart_->addAxis(x_axis_, Qt::AlignBottom);

    y_axis_ = new QValueAxis();
    y_axis_->setLabelsColor(QColor("#787b86"));
    y_axis_->setGridLineColor(QColor("#2a2e39"));
    y_axis_->setLinePen(QPen(Qt::NoPen));
    chart_->addAxis(y_axis_, Qt::AlignRight);

    switch (type) {
    case RSI: setup_rsi_chart(); break;
    case MACD: setup_macd_chart(); break;
    case Stochastic: setup_stoch_chart(); break;
    }
}

void IndicatorPanel::setup_rsi_chart() {
    main_series_ = new QLineSeries();
    main_series_->setPen(QPen(QColor("#787b86"), 1.5));
    chart_->addSeries(main_series_);
    main_series_->attachAxis(x_axis_);
    main_series_->attachAxis(y_axis_);
    y_axis_->setRange(0, 100);
    y_axis_->setTickCount(4);

    overbought_ = new QLineSeries();
    overbought_->setPen(QPen(QColor("#f23645"), 1, Qt::DashLine));
    chart_->addSeries(overbought_);
    overbought_->attachAxis(x_axis_);
    overbought_->attachAxis(y_axis_);

    oversold_ = new QLineSeries();
    oversold_->setPen(QPen(QColor("#089981"), 1, Qt::DashLine));
    chart_->addSeries(oversold_);
    oversold_->attachAxis(x_axis_);
    oversold_->attachAxis(y_axis_);
}

void IndicatorPanel::setup_macd_chart() {
    main_series_ = new QLineSeries();
    main_series_->setPen(QPen(QColor("#2962ff"), 1.5));
    chart_->addSeries(main_series_);
    main_series_->attachAxis(x_axis_);
    main_series_->attachAxis(y_axis_);

    signal_series_ = new QLineSeries();
    signal_series_->setPen(QPen(QColor("#f23645"), 1.5));
    chart_->addSeries(signal_series_);
    signal_series_->attachAxis(x_axis_);
    signal_series_->attachAxis(y_axis_);

    // Histogram bars are added dynamically in set_macd_data
}

void IndicatorPanel::setup_stoch_chart() {
    main_series_ = new QLineSeries();
    main_series_->setPen(QPen(QColor("#2962ff"), 1.5));
    chart_->addSeries(main_series_);
    main_series_->attachAxis(x_axis_);
    main_series_->attachAxis(y_axis_);
    y_axis_->setRange(0, 100);
    y_axis_->setTickCount(4);

    signal_series_ = new QLineSeries();
    signal_series_->setPen(QPen(QColor("#f23645"), 1, Qt::DashLine));
    chart_->addSeries(signal_series_);
    signal_series_->attachAxis(x_axis_);
    signal_series_->attachAxis(y_axis_);

    overbought_ = new QLineSeries();
    overbought_->setPen(QPen(QColor("#787b86"), 1, Qt::DashLine));
    chart_->addSeries(overbought_);
    overbought_->attachAxis(x_axis_);
    overbought_->attachAxis(y_axis_);

    oversold_ = new QLineSeries();
    oversold_->setPen(QPen(QColor("#787b86"), 1, Qt::DashLine));
    chart_->addSeries(oversold_);
    oversold_->attachAxis(x_axis_);
    oversold_->attachAxis(y_axis_);
}

void IndicatorPanel::set_data(const QVector<double>& timestamps_ms,
                               const QVector<double>& values) {
    if (!main_series_) return;
    main_series_->clear();
    QVector<QPointF> pts;
    double min_y = 1e18, max_y = -1e18;
    for (int i = 0; i < timestamps_ms.size() && i < values.size(); ++i) {
        if (values[i] != 0 || i == timestamps_ms.size() - 1) {
            pts.append(QPointF(timestamps_ms[i], values[i]));
            min_y = std::min(min_y, values[i]);
            max_y = std::max(max_y, values[i]);
        }
    }
    main_series_->replace(pts);

    if (overbought_) {
        overbought_->clear();
        if (!timestamps_ms.isEmpty()) {
            overbought_->append(timestamps_ms.first(), type_ == RSI ? 70 : 80);
            overbought_->append(timestamps_ms.last(), type_ == RSI ? 70 : 80);
        }
    }
    if (oversold_) {
        oversold_->clear();
        if (!timestamps_ms.isEmpty()) {
            oversold_->append(timestamps_ms.first(), type_ == RSI ? 30 : 20);
            oversold_->append(timestamps_ms.last(), type_ == RSI ? 30 : 20);
        }
    }

    // MACD: auto-scale with padding
    if (type_ == RSI) { y_axis_->setRange(0, 100); }
    else if (!pts.isEmpty() && min_y < max_y) {
        double pad = (max_y - min_y) * 0.1;
        y_axis_->setRange(min_y - pad, max_y + pad);
    }
}

void IndicatorPanel::set_macd_data(const QVector<double>& timestamps_ms,
                                    const QVector<double>& macd_vals,
                                    const QVector<double>& signal_vals,
                                    const QVector<double>& hist_vals) {
    if (!main_series_) return;
    main_series_->clear();
    signal_series_->clear();

    QVector<QPointF> macd_pts, signal_pts;
    double min_y = 1e18, max_y = -1e18;
    for (int i = 0; i < timestamps_ms.size() && i < macd_vals.size(); ++i) {
        if (macd_vals[i] != 0 || i == timestamps_ms.size() - 1) {
            macd_pts.append(QPointF(timestamps_ms[i], macd_vals[i]));
            min_y = std::min(min_y, macd_vals[i]);
            max_y = std::max(max_y, macd_vals[i]);
        }
    }
    for (int i = 0; i < timestamps_ms.size() && i < signal_vals.size(); ++i) {
        if (signal_vals[i] != 0 || i == timestamps_ms.size() - 1) {
            signal_pts.append(QPointF(timestamps_ms[i], signal_vals[i]));
        }
    }
    main_series_->replace(macd_pts);
    signal_series_->replace(signal_pts);

    // Remove old histogram bars
    auto old_bars = chart_->series();
    for (auto* s : old_bars) {
        if (s != main_series_ && s != signal_series_) {
            chart_->removeSeries(s);
            delete s;
        }
    }

    if (!timestamps_ms.isEmpty()) {
        auto* hist_set = new QBarSet("Hist");
        auto* hist_series = new QBarSeries();
        for (int i = 0; i < timestamps_ms.size() && i < hist_vals.size(); ++i) {
            *hist_set << hist_vals[i];
        }
        hist_series->append(hist_set);
        hist_series->setBarWidth(0.8);
        QPen no_pen(Qt::NoPen);
        hist_set->setColor(QColor("#2962ff"));
        hist_set->setBorderColor(Qt::transparent);
        chart_->addSeries(hist_series);
        hist_series->attachAxis(x_axis_);
        hist_series->attachAxis(y_axis_);
    }

    if (macd_pts.isEmpty()) return;
    double pad = (max_y - min_y) * 0.2;
    if (pad < 1) pad = 1;
    y_axis_->setRange(min_y - pad, max_y + pad);
}

void IndicatorPanel::set_stoch_data(const QVector<double>& timestamps_ms,
                                     const QVector<double>& k_vals,
                                     const QVector<double>& d_vals) {
    if (!main_series_) return;
    main_series_->clear();
    signal_series_->clear();

    QVector<QPointF> k_pts, d_pts;
    for (int i = 0; i < timestamps_ms.size() && i < k_vals.size(); ++i) {
        if (k_vals[i] != 0 || i == timestamps_ms.size() - 1)
            k_pts.append(QPointF(timestamps_ms[i], k_vals[i]));
    }
    for (int i = 0; i < timestamps_ms.size() && i < d_vals.size(); ++i) {
        if (d_vals[i] != 0 || i == timestamps_ms.size() - 1)
            d_pts.append(QPointF(timestamps_ms[i], d_vals[i]));
    }
    main_series_->replace(k_pts);
    signal_series_->replace(d_pts);
    y_axis_->setRange(0, 100);
}

void IndicatorPanel::update_x_range(qint64 min_ms, qint64 max_ms) {
    x_axis_->setRange(QDateTime::fromMSecsSinceEpoch(min_ms),
                      QDateTime::fromMSecsSinceEpoch(max_ms));
}

void IndicatorPanel::clear() {
    if (main_series_) main_series_->clear();
    if (signal_series_) signal_series_->clear();
    if (overbought_) overbought_->clear();
    if (oversold_) oversold_->clear();
}

} // namespace fincept::ui
