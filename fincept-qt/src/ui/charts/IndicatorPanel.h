#pragma once

#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QVector>

class QChart;

namespace fincept::ui {

class IndicatorPanel : public QChartView {
    Q_OBJECT
public:
    enum Type { RSI, MACD, Stochastic };

    IndicatorPanel(Type type, int height = 120, QWidget* parent = nullptr);

    void set_data(const QVector<double>& timestamps_ms,
                  const QVector<double>& values);
    void set_macd_data(const QVector<double>& timestamps_ms,
                       const QVector<double>& macd,
                       const QVector<double>& signal,
                       const QVector<double>& histogram);
    void set_stoch_data(const QVector<double>& timestamps_ms,
                        const QVector<double>& k,
                        const QVector<double>& d);

    void update_x_range(qint64 min_ms, qint64 max_ms);
    void clear();

    Type type() const { return type_; }

signals:
    void remove_requested();

private:
    void setup_rsi_chart();
    void setup_macd_chart();
    void setup_stoch_chart();

    Type type_;
    QChart* chart_ = nullptr;
    QDateTimeAxis* x_axis_ = nullptr;
    QValueAxis* y_axis_ = nullptr;

    QLineSeries* main_series_ = nullptr;
    QLineSeries* signal_series_ = nullptr;  // MACD signal / Stoch %D
    class QBarSeries* histogram_ = nullptr; // MACD histogram
    QLineSeries* overbought_ = nullptr;
    QLineSeries* oversold_ = nullptr;
};

} // namespace fincept::ui
