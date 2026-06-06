// MT5FleetChartPanel.h — Enhanced chart: 10 chart types, 17 drawing tools, RSI/MACD sub-pane, bar replay, alerts, volume profile, time sessions, crosshair, keyboard shortcuts, templates, screenshot, fullscreen, date range, context menu, bar coloring
#pragma once
#include <QWidget>
#include <QChartView>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QLineSeries>
#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QMouseEvent>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QCheckBox>
#include <QMenu>
#include <QDateEdit>
#include <QShortcut>
#include <QWebSocket>
#include "screens/algo_trading/ChartTypeTransform.h"
#include "screens/algo_trading/IndicatorParamDialog.h"
#include "trading/SmartOrderEngine.h"
#include "screens/crypto_trading/CryptoChart.h"

namespace fincept::screens {

class DrawingObject;
class IndicatorPane;
class VolumeProfileLayer;
class BarReplayEngine;
class ChartAlertManager;
class TimeSessionManager;
class EconomicEventOverlay;
struct ChartAlert;

// ── DrawingChartView — captures mouse for tool placement ────────

class DrawingChartView : public QChartView {
    Q_OBJECT
  public:
    enum class Mode { Normal, Placing, Dragging, Pan };
    explicit DrawingChartView(QChart* chart, QWidget* parent = nullptr);
    void setDrawingMode(Mode mode) { mode_ = mode; }
    Mode drawingMode() const { return mode_; }
    void setActiveTool(const QString& toolName);
    QString activeTool() const { return active_tool_; }
    void setCrosshairEnabled(bool on);
    bool crosshairEnabled() const { return crosshair_enabled_; }

  signals:
    void objectPlaced(QPointF start, QPointF end);
    void pointPlaced(QPointF pt);
    void drawingFinished();
    void mouseMoved(QPointF chartPos, QPointF viewPos);

  protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

  private:
    Mode mode_ = Mode::Normal;
    QString active_tool_;
    QPointF press_pos_;
    QPointF current_pos_;
    bool is_placing_ = false;
    bool crosshair_enabled_ = false;
    QGraphicsLineItem* rubber_band_ = nullptr;
};

// ── Struct for order markers ────────────────────────────────────

struct OrderMarker {
    qint64 time = 0;
    double price = 0;
    QString side;
    QString label;
    double lot = 0;
};

// ── Main Chart Panel ────────────────────────────────────────────

class MT5FleetChartPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetChartPanel(QWidget* parent = nullptr);
    ~MT5FleetChartPanel() override;
    void set_symbol(const QString& symbol);
    void set_timeframe(const QString& tf);
    void set_order_markers(const QVector<OrderMarker>& markers);

  private slots:
    void on_timeframe_changed(int idx);
    void on_indicator_changed(int idx);
    void on_drawing_tool_clicked();
    void on_compare_symbol_changed(int idx);
    void on_log_scale_toggled(bool checked);
    void refresh_chart();
    void save_drawings();
    void load_drawings();
    void clear_drawing_tools();
    void undo_last_tool();
    void on_chart_type_changed(int idx);
    void on_crosshair_toggled();
    void on_fullscreen_toggled();
    void take_screenshot();
    void on_chart_template_save();
    void on_chart_template_load();
    void on_bar_coloring_toggled();
    void on_vp_toggled();
    void on_session_toggled();
    void on_econ_toggled();

    // Replay
    void on_replay_play();
    void on_replay_pause();
    void on_replay_stop();
    void on_replay_step_fwd();
    void on_replay_step_back();
    void on_replay_bar_changed(int index);

    // Alerts
    void on_add_alert();
    void on_alert_triggered(const ChartAlert& alert);
    void on_paper_trade();
    void refresh_positions();

    // Date range
    void on_date_range_apply();

  private:
    void build_ui();
    void apply_theme();
    void load_chart_data();
    void load_compare_data(const QString& compareSymbol);
    void render_volume_bars();
    void render_price_line();
    void render_order_markers();
    void add_indicator(const QString& name);
    void add_oscillator_indicator(const QString& name);
    void clear_indicators();
    void set_active_tool(const QString& toolName);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void handle_object_placed(QPointF start, QPointF end);
    void handle_point_placed(QPointF pt);
    void handle_drawing_finished();
    void on_hover_position(const QPointF& chartVal, const QPointF& viewPos);
    void on_hover_leave();
    void update_last_price_marker();
    void apply_chart_type_transform();
    void render_time_sessions();
    void render_econ_events();
    void render_alerts();
    void render_alerts_on_chart();
    void edit_indicator_params();
    void setup_shortcuts();
    void show_chart_context_menu(const QPoint& pos);

    // Chart — CryptoChart with TradingView-like crosshair/OHLC tooltip
    QWidget* chart_container_ = nullptr;
    crypto::CryptoChart* crypto_chart_ = nullptr;
    QChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    IndicatorPane* indicator_pane_ = nullptr;
    // Crosshair overlay items (like CryptoChart)
    QGraphicsLineItem* xhair_v_ = nullptr;
    QGraphicsLineItem* xhair_h_ = nullptr;
    QGraphicsEllipseItem* xhair_dot_ = nullptr;
    QGraphicsRectItem* price_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* price_tag_txt_ = nullptr;
    QGraphicsRectItem* time_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* time_tag_txt_ = nullptr;
    // Last-price tag (always-visible on right axis, like CryptoChart)
    QGraphicsRectItem* last_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* last_tag_txt_ = nullptr;

    QLabel* ohlc_tooltip_ = nullptr;

    QLineSeries* price_series_ = nullptr;
    QCandlestickSeries* candlestick_series_ = nullptr;
    QLineSeries* volume_series_ = nullptr;
    QBarSeries* volume_bars_ = nullptr;
    QBarSet* volume_up_set_ = nullptr;
    QBarSet* volume_down_set_ = nullptr;
    QLineSeries* price_line_series_ = nullptr;
    QLineSeries* compare_series_ = nullptr;
    QVector<QGraphicsSimpleTextItem*> order_text_items_;
    QVector<QLineSeries*> order_marker_series_;
    QVector<QLineSeries*> indicator_series_;
    QVector<DrawingObject*> drawing_objects_;
    QVector<QPointF> elliott_points_;
    QVector<QGraphicsRectItem*> time_session_rects_;
    QVector<QGraphicsSimpleTextItem*> econ_event_items_;
    QVector<QGraphicsItem*> alert_marker_items_;

    QVector<OhlcvPoint> ohlc_data_;
    QVector<OhlcvPoint> transformed_data_;
    QVector<OrderMarker> order_markers_;
    QValueAxis* volume_axis_ = nullptr;

    // Low-latency execution engine
    trading::SmartOrderEngine* order_engine_ = nullptr;

    // New subsystems
    VolumeProfileLayer* volume_profile_ = nullptr;
    BarReplayEngine* replay_engine_ = nullptr;
    ChartAlertManager* alert_manager_ = nullptr;
    TimeSessionManager* session_manager_ = nullptr;
    EconomicEventOverlay* econ_overlay_ = nullptr;
    IndicatorParamDialog::IndicatorParams indicator_params_;

    // Controls
    QComboBox* timeframe_combo_ = nullptr;
    QComboBox* indicator_combo_ = nullptr;
    QComboBox* chart_style_combo_ = nullptr;
    QComboBox* chart_type_combo_ = nullptr;
    QComboBox* compare_combo_ = nullptr;
    QLabel* symbol_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* tool_label_ = nullptr;
    QLabel* replay_label_ = nullptr;
    QLabel* data_tooltip_label_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* clear_draw_btn_ = nullptr;
    QPushButton* undo_draw_btn_ = nullptr;
    QPushButton* save_draw_btn_ = nullptr;
    QPushButton* load_draw_btn_ = nullptr;
    QPushButton* text_label_btn_ = nullptr;
    QCheckBox* log_scale_check_ = nullptr;
    QMenu* context_menu_ = nullptr;

    // New feature buttons
    QPushButton* crosshair_btn_ = nullptr;
    QPushButton* fullscreen_btn_ = nullptr;
    QPushButton* screenshot_btn_ = nullptr;
    QPushButton* vp_btn_ = nullptr;
    QPushButton* session_btn_ = nullptr;
    QPushButton* econ_btn_ = nullptr;
    QPushButton* alert_btn_ = nullptr;
    QPushButton* bar_coloring_btn_ = nullptr;
    QPushButton* paper_trade_btn_ = nullptr;
    QPushButton* save_template_btn_ = nullptr;
    QPushButton* load_template_btn_ = nullptr;
    QPushButton* indicator_params_btn_ = nullptr;
    QDateEdit* date_from_ = nullptr;
    QDateEdit* date_to_ = nullptr;
    QPushButton* date_apply_btn_ = nullptr;

    // Replay controls
    QWidget* replay_bar_ = nullptr;
    QPushButton* replay_play_btn_ = nullptr;
    QPushButton* replay_stop_btn_ = nullptr;
    QPushButton* replay_step_fwd_btn_ = nullptr;
    QPushButton* replay_step_back_btn_ = nullptr;

    // Drawing tool buttons
    QPushButton* trendline_btn_ = nullptr;
    QPushButton* hline_btn_ = nullptr;
    QPushButton* vline_btn_ = nullptr;
    QPushButton* channel_btn_ = nullptr;
    QPushButton* fib_btn_ = nullptr;
    QPushButton* fib_ext_btn_ = nullptr;
    QPushButton* fib_fan_btn_ = nullptr;
    QPushButton* gann_fan_btn_ = nullptr;
    QPushButton* elliott_btn_ = nullptr;

    enum class ChartStyle { Candlestick, Bar, Line, Area };
    void set_chart_style(ChartStyle style);
    ChartStyle chart_style_ = ChartStyle::Candlestick;

    // State
    QString current_symbol_ = "XAUUSD";
    QString current_timeframe_ = "H1";
    QString current_indicator_ = "None";
    QString current_compare_symbol_ = "";
    QString active_tool_name_ = "None";
    QString current_chart_type_ = "None";
    QVector<QPointF> chart_data_;
    bool is_placing_ = false;
    bool is_elliott_mode_ = false;
    bool is_text_label_mode_ = false;
    bool log_scale_enabled_ = false;
    bool fullscreen_enabled_ = false;
    bool crosshair_enabled_ = false;
    bool bar_coloring_enabled_ = false;
    bool vp_enabled_ = false;
    bool session_enabled_ = false;
    bool econ_enabled_ = false;
    QPointF last_start_;
    QDateTimeAxis* axis_x_ = nullptr;
    QAbstractAxis* axis_y_ = nullptr;
};

} // namespace fincept::screens
