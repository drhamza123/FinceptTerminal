#pragma once
// Crypto Chart — Candlestick chart with timeframe toggles, crosshair,
// OHLC tooltip, and a last-price tag pinned to the right axis.

#include "trading/TradingTypes.h"
#include "ui/charts/ChartOverlayManager.h"
#include "ui/charts/PositionLayer.h"
#include "ui/charts/IndicatorPanel.h"
#include "ui/drawing/DrawingManager.h"
#include "trading/IndicatorCalculator.h"

#include <QPushButton>
#include <QVector>
#include <QWidget>

class QChart;
class QCandlestickSeries;
class QLineSeries;
class QDateTimeAxis;
class QValueAxis;
class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsEllipseItem;
class QGraphicsSimpleTextItem;
class QLabel;
class QVBoxLayout;
class QWheelEvent;

namespace fincept::ui { class IndicatorPicker; }

namespace fincept::screens::crypto {

class CryptoChart;

class HoverChartView : public QChartView {
  public:
    HoverChartView(QChart* chart, CryptoChart* host);
  protected:
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
  private:
    CryptoChart* host_ = nullptr;
};

class CryptoChart : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoChart(QWidget* parent = nullptr);

    void set_candles(const QVector<trading::Candle>& candles);
    void append_candle(const trading::Candle& candle);
    void clear();
    void update_positions(const QVector<fincept::ui::PositionLevel>& positions);

    QString current_timeframe() const;
    fincept::ui::ChartOverlayManager* overlay_manager() const { return overlay_mgr_; }
    QChart* chart() const { return chart_; }
    QWidget* chartView() const { return chart_view_; }
    void update_indicator_panels();
    void update_volume_profile();

  signals:
    void timeframe_changed(const QString& tf);
    void position_sl_tp_changed(const QString& order_id, double new_sl, double new_tp);

  private:
    void rebuild_chart();
    void update_axes(double min_price, double max_price, qint64 min_time, qint64 max_time);
    void set_active_tf(int idx);
    void apply_tf_axis_format();
    void update_last_price_marker();
    void on_hover_position(const QPointF& chart_value_pos, const QPoint& view_pos);
    void on_hover_leave();

    HoverChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QCandlestickSeries* series_ = nullptr;
    friend class HoverChartView;
    QDateTimeAxis* time_axis_ = nullptr;
    QValueAxis* price_axis_ = nullptr;
    QLineSeries* last_price_line_ = nullptr;

    // Crosshair / hover overlay (live in the chart's QGraphicsScene)
    QGraphicsLineItem* xhair_v_ = nullptr;
    QGraphicsLineItem* xhair_h_ = nullptr;
    QGraphicsEllipseItem* xhair_dot_ = nullptr;
    QGraphicsRectItem* price_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* price_tag_txt_ = nullptr;
    QGraphicsRectItem* time_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* time_tag_txt_ = nullptr;

    // Last-price tag (always-visible, tracks the latest close)
    QGraphicsRectItem* last_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* last_tag_txt_ = nullptr;

    // OHLC tooltip pinned to the chart's top-left corner
    QLabel* ohlc_tooltip_ = nullptr;

    // Timeframe toggle buttons (31 timeframes)
    QPushButton* tf_buttons_[31] = {};
    int active_tf_ = 9; // default "1h"
    static constexpr const char* TF_LABELS[] = {
        "M1","M2","M3","M4","M5","M6","M10","M12","M15","M20",
        "H1","H2","H3","H4","H6","H8","H12",
        "D1","D2","D3","D5",
        "W1","W2","W3",
        "MN1","MN2","MN3","MN6",
        "Y1","Y2","Y5"
    };
    // Map label -> backend timeframe string
    static QString tf_label_to_backend(const QString& lbl);

    QVector<trading::Candle> candles_;
    static constexpr int MAX_VISIBLE = 120;

    // Axis range cache (padded values actually applied to the axis)
    double last_min_price_ = -1;
    double last_max_price_ = -1;
    qint64 last_min_time_ = -1;
    qint64 last_max_time_ = -1;

    // Incremental price bounds over visible candle set — avoids O(N) rescans
    // on every intrabar tick. -1 means "stale, recompute on next append".
    double cached_min_price_ = -1;
    double cached_max_price_ = -1;
    bool bounds_dirty_ = true;
    void recompute_bounds();

    // Pending timeframe request while a fetch is already in-flight
    // set_candles() will emit timeframe_changed again if this is set
    QString pending_tf_;

    fincept::ui::ChartOverlayManager* overlay_mgr_ = nullptr;
    fincept::ui::IndicatorPicker* indicator_picker_ = nullptr;
    fincept::ui::PositionLayer* position_layer_ = nullptr;

    friend class HoverChartView;

    // Indicator sub-panels (RSI, MACD, Stochastic)
    QVBoxLayout* indicator_panels_ = nullptr;
    fincept::ui::IndicatorPanel* rsi_panel_ = nullptr;
    fincept::ui::IndicatorPanel* macd_panel_ = nullptr;
    fincept::ui::IndicatorPanel* stoch_panel_ = nullptr;
    class VolumeProfileLayer* vol_profile_widget_ = nullptr;
    fincept::screens::VolumeProfileLayer* vol_profile_panel_ = nullptr;

    // Chart type mode (0=candlestick, 1=renko, 2=kagi, 3=pnf)
    int chart_mode_ = 0;
    QPushButton* chart_mode_btn_ = nullptr;
    void cycle_chart_mode();
    // Chart style (0=candle, 1=bar, 2=line, 3=area)
    int chart_style_ = 0;
    QPushButton* chart_style_btn_ = nullptr;
    void cycle_chart_style();
    void apply_chart_style();
    QLineSeries* line_style_series_ = nullptr;
    // Log scale
    bool log_scale_ = false;
    QPushButton* log_scale_btn_ = nullptr;

    // Drawing tools
    QPushButton* draw_toggle_ = nullptr;
    QWidget* draw_toolbar_ = nullptr;
    int active_draw_tool_ = -1;
    bool draw_placing_ = false;
    QPointF draw_start_pt_;
    QVector<class QGraphicsItem*> draw_items_;
    void toggle_draw_toolbar();
    void on_draw_tool_clicked(int tool);
    void clear_drawings();
    void place_drawing(const QPointF& chart_pos);
    fincept::ui::DrawingManager* drawing_mgr_ = nullptr;

    // Volume Footprint + Renko + Position Sizing + Kagi + P&F
    QPushButton* vfp_toggle_ = nullptr;
    QPushButton* renko_toggle_ = nullptr;
    QPushButton* kagi_toggle_ = nullptr;
    QPushButton* pnf_toggle_ = nullptr;
    class VolumeFootprint* vfp_widget_ = nullptr;
    class RenkoChart* renko_widget_ = nullptr;
    class KagiChart* kagi_widget_ = nullptr;
    class PointFigureChart* pnf_widget_ = nullptr;
    class PositionSizingTool* sizing_tool_ = nullptr;
    QWidget* chart_overlay_stack_ = nullptr;
    void toggle_vfp();
    void toggle_renko();
    void toggle_kagi();
    void toggle_pnf();
    void toggle_sizing();
};

} // namespace fincept::screens::crypto
