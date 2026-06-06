// IndicatorPane.cpp — Oscillator sub-chart rendering
#include "screens/algo_trading/IndicatorPane.h"
#include "ui/theme/Theme.h"

#include <QPen>
#include <QBrush>

namespace fincept::screens {

IndicatorPane::IndicatorPane(QWidget* parent) : QWidget(parent) {
    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(0);

    label_ = new QLabel("", this);
    label_->setFixedHeight(20);
    label_->setStyleSheet(QString("color:%1;font-size:11px;padding:2px 8px;background:%2;")
        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_SURFACE()));
    layout_->addWidget(label_);

    chart_ = new QChart();
    chart_->legend()->hide();
    chart_->setMargins(QMargins(0, 0, 0, 0));
    chart_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundVisible(true);

    chart_view_ = new QChartView(chart_, this);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    chart_view_->setFixedHeight(160);
    layout_->addWidget(chart_view_);

    hide();
}

void IndicatorPane::applyTheme() {
    chart_->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    if (axis_x_) {
        axis_x_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
        axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    }
    if (axis_y_) {
        axis_y_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
        axis_y_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    }
}

void IndicatorPane::showIndicator(const QString& name, const QVector<QPointF>& data) {
    clear();
    current_indicator_ = name;
    label_->setText(name);

    delete series_; series_ = nullptr;
    series_ = new QLineSeries();
    series_->setName(name);

    QColor lineColor(100, 200, 255);
    if (name == "RSI") lineColor = QColor(255, 100, 100);
    else if (name == "MACD") lineColor = QColor(100, 200, 255);
    else if (name == "Stochastic") lineColor = QColor(200, 100, 255);
    else if (name == "ADX") lineColor = QColor(255, 200, 100);
    series_->setPen(QPen(lineColor, 1.5));

    qreal min_y = 1e9, max_y = -1e9;
    for (const auto& pt : data) {
        series_->append(pt);
        min_y = qMin(min_y, pt.y());
        max_y = qMax(max_y, pt.y());
    }

    chart_->addSeries(series_);

    // Time axis (no labels — kept clean)
    delete axis_x_; axis_x_ = nullptr;
    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("dd HH:mm");
    axis_x_->setLabelsVisible(false);
    axis_x_->setGridLineVisible(true);
    axis_x_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    series_->attachAxis(axis_x_);

    // Value axis — auto range per indicator type
    delete axis_y_; axis_y_ = nullptr;
    axis_y_ = new QValueAxis();
    axis_y_->setLabelFormat("%.1f");
    axis_y_->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    axis_y_->setGridLineColor(QColor(ui::colors::BORDER_DIM()));

    if (name == "RSI") {
        axis_y_->setRange(0, 100);
    } else if (name == "Stochastic") {
        axis_y_->setRange(0, 100);
    } else if (name == "ADX") {
        axis_y_->setRange(0, 100);
    } else {
        double pad = (max_y - min_y) * 0.1;
        if (pad < 0.1) pad = 1.0;
        axis_y_->setRange(min_y - pad, max_y + pad);
    }

    // Must add axis to chart before attaching any series to it
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    series_->attachAxis(axis_y_);

    // Level lines — added after axis_y_ is registered with chart
    if (name == "RSI") {
        addLevelLine("OB", 70, QColor(255, 80, 80, 80));
        addLevelLine("OS", 30, QColor(80, 255, 80, 80));
    } else if (name == "Stochastic") {
        addLevelLine("OB", 80, QColor(255, 80, 80, 80));
        addLevelLine("OS", 20, QColor(80, 255, 80, 80));
    } else if (name == "ADX") {
        addLevelLine("Strong", 25, QColor(255, 200, 80, 80));
    }

    applyTheme();
    show();
}

void IndicatorPane::addLevelLine(const QString& name, double level, const QColor& color) {
    auto* line = new QLineSeries();
    line->setName(name);
    line->setPen(QPen(color, 1, Qt::DashLine));
    // Add two points — min/max timestamps will be set when X range is known
    line->append(0, level);
    line->append(1, level);
    chart_->addSeries(line);
    if (axis_x_) line->attachAxis(axis_x_);
    if (axis_y_) line->attachAxis(axis_y_);
    level_lines_.append(line);
}

void IndicatorPane::clear() {
    series_ = nullptr;
    for (auto* s : level_lines_) {
        chart_->removeSeries(s);
        delete s;
    }
    level_lines_.clear();
    chart_->removeAllSeries();
    delete axis_x_; axis_x_ = nullptr;
    delete axis_y_; axis_y_ = nullptr;
    current_indicator_.clear();
    hide();
}

void IndicatorPane::linkXAxis(QDateTimeAxis* masterAxis) {
    if (!masterAxis) return;
    connect(masterAxis, &QDateTimeAxis::rangeChanged, this,
        [this](QDateTime min, QDateTime max) {
            if (axis_x_) axis_x_->setRange(min, max);
        });
}

void IndicatorPane::setXRange(const QDateTime& min, const QDateTime& max) {
    if (axis_x_) axis_x_->setRange(min, max);
}

} // namespace fincept::screens
