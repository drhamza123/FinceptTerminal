// IndicatorPane.h — Oscillator sub-chart (RSI, MACD, Stochastic, ADX)
#pragma once
#include <QWidget>
#include <QChartView>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {

class IndicatorPane : public QWidget {
    Q_OBJECT
  public:
    explicit IndicatorPane(QWidget* parent = nullptr);
    ~IndicatorPane() override = default;

    void showIndicator(const QString& name, const QVector<QPointF>& data);
    void clear();
    void linkXAxis(QDateTimeAxis* masterAxis);
    void setXRange(const QDateTime& min, const QDateTime& max);
    QChartView* chartView() const { return chart_view_; }

  private:
    void applyTheme();
    void addLevelLine(const QString& name, double level, const QColor& color);

    QVBoxLayout* layout_;
    QChartView* chart_view_;
    QChart* chart_;
    QLineSeries* series_ = nullptr;
    QValueAxis* axis_y_ = nullptr;
    QDateTimeAxis* axis_x_ = nullptr;
    QLabel* label_ = nullptr;
    QVector<QLineSeries*> level_lines_;
    QString current_indicator_;
};

} // namespace fincept::screens
