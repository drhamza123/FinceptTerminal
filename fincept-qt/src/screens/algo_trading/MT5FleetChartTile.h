// MT5FleetChartTile.h — Independent chart tile for multi-chart layouts
#pragma once
#include <QWidget>
#include <QChart>
#include <QChartView>
#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QPointF>

namespace fincept::screens {

enum class ChartStyle { Candlestick, Bar, Line, Area };

struct OhlcPoint {
    qreal time = 0, open = 0, high = 0, low = 0, close = 0, volume = 0;
};

class MT5FleetChartTile : public QWidget {
    Q_OBJECT
public:
    explicit MT5FleetChartTile(int index, QWidget* parent = nullptr);
    ~MT5FleetChartTile() override;

    void setSymbol(const QString& symbol);
    void setTimeframe(const QString& tf);
    void setChartStyle(ChartStyle style);
    ChartStyle chartStyle() const { return style_; }
    QString symbol() const { return symbol_; }
    QString timeframe() const { return timeframe_; }

    void loadData();
    void clearData();

signals:
    void symbolChanged(const QString& symbol);
    void timeframeChanged(const QString& tf);

private:
    void build_ui();
    void apply_theme();
    void render_candlestick();
    void render_bar();
    void render_line();
    void render_area();
    void clear_series();

    int index_;
    QString symbol_ = "XAUUSD";
    QString timeframe_ = "H1";
    ChartStyle style_ = ChartStyle::Candlestick;
    QVector<OhlcPoint> data_;

    QChart* chart_ = nullptr;
    QChartView* chart_view_ = nullptr;
    QLabel* title_label_ = nullptr;
    QComboBox* style_combo_ = nullptr;
    QPushButton* sync_btn_ = nullptr;

    QDateTimeAxis* axis_x_ = nullptr;
    QValueAxis* axis_y_ = nullptr;
    bool synced_ = false;
};

} // namespace fincept::screens
