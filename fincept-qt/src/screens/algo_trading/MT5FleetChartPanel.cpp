// MT5FleetChartPanel.cpp — Full-featured chart: 10 chart types, 17 drawing tools, multi-pane indicators, bar replay, alerts, volume profile, time sessions, crosshair, keyboard shortcuts, templates, screenshot, fullscreen, date range, context menu, bar coloring
#include "screens/algo_trading/MT5FleetChartPanel.h"
#include "screens/algo_trading/DrawingObject.h"
#include "screens/algo_trading/IndicatorPane.h"
#include "screens/algo_trading/ChartTypeTransform.h"
#include "screens/algo_trading/VolumeProfileLayer.h"
#include "screens/algo_trading/BarReplayEngine.h"
#include "screens/algo_trading/ChartAlertManager.h"
#include "screens/algo_trading/TimeSessionMarker.h"
#include "screens/algo_trading/EconomicEventMarker.h"
#include "screens/algo_trading/IndicatorParamDialog.h"
#include "screens/algo_trading/PolygonOverlay.h"
#include "screens/algo_trading/ExecutionPanel.h"
#include "network/http/HttpClient.h"
#include "core/config/AppConfig.h"
#include "ui/theme/Theme.h"

#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QLogValueAxis>
#include <QValueAxis>
#include <QLineSeries>
#include <QCandlestickSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QBarCategoryAxis>
#include <QAreaSeries>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QMessageBox>
#include <QPen>
#include <QBrush>
#include <QInputDialog>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QColorDialog>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::screens {

static const int MAX_POINTS = 200;

static QString mt5_ws_url(const QString& path) {
    QString base = fincept::AppConfig::instance().api_base_url();
    if (base.contains(":8155")) base.replace(":8155", ":8156");
    QUrl url(base);
    url.setScheme(url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("wss")
        : QStringLiteral("ws"));
    url.setPath(path);
    return url.toString();
}

static QString mt5_ohlc_path(const QString& symbol, const QString& timeframe, int count) {
    QUrl url(QStringLiteral("/mt5/market/ohlc"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("symbol"), symbol);
    q.addQueryItem(QStringLiteral("timeframe"), timeframe);
    q.addQueryItem(QStringLiteral("count"), QString::number(count));
    url.setQuery(q);
    return url.toString();
}

static bool is_oscillator(const QString& name) {
    return name == "RSI" || name == "MACD" || name == "Stochastic" || name == "ADX";
}

static qreal axisMin(QAbstractAxis* a) {
    if (auto* v = qobject_cast<QValueAxis*>(a)) return v->min();
    if (auto* l = qobject_cast<QLogValueAxis*>(a)) return l->min();
    return 0;
}
static qreal axisMax(QAbstractAxis* a) {
    if (auto* v = qobject_cast<QValueAxis*>(a)) return v->max();
    if (auto* l = qobject_cast<QLogValueAxis*>(a)) return l->max();
    return 1;
}

// ═══════════════════════════════════════════════════════════════
// DrawingChartView
// ═══════════════════════════════════════════════════════════════

DrawingChartView::DrawingChartView(QChart* chart, QWidget* parent)
    : QChartView(chart, parent) {
    setRubberBand(QChartView::RectangleRubberBand);
    setRenderHint(QPainter::Antialiasing);
    setMouseTracking(true);
}

void DrawingChartView::setCrosshairEnabled(bool on) {
    crosshair_enabled_ = on;
    if (on) {
        setCursor(Qt::CrossCursor);
        setRubberBand(QChartView::NoRubberBand);
    } else {
        setCursor(Qt::ArrowCursor);
        setRubberBand(QChartView::RectangleRubberBand);
    }
    update();
}

void DrawingChartView::setActiveTool(const QString& toolName) {
    active_tool_ = toolName;
    if (toolName == "None" || toolName.isEmpty()) {
        if (!crosshair_enabled_) {
            setDrawingMode(Mode::Normal);
            setCursor(Qt::ArrowCursor);
            setRubberBand(QChartView::RectangleRubberBand);
        }
    } else {
        setDrawingMode(Mode::Placing);
        setCursor(Qt::CrossCursor);
        setRubberBand(QChartView::NoRubberBand);
    }
}

void DrawingChartView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        QPointF chartPos = chart()->mapToValue(event->position());
        emit mouseMoved(chartPos, event->position());
        QChartView::mousePressEvent(event);
        return;
    }
    if (mode_ != Mode::Placing) {
        QChartView::mousePressEvent(event);
        return;
    }
    press_pos_ = event->position();
    is_placing_ = true;
    if (active_tool_ == "Elliott Wave" || active_tool_ == "Text") {
        QPointF scene_pt = chart()->mapToValue(press_pos_);
        emit pointPlaced(scene_pt);
    } else {
        if (!rubber_band_) {
            rubber_band_ = new QGraphicsLineItem();
            rubber_band_->setPen(QPen(QColor(255,255,255,128), 1, Qt::DashLine));
            scene()->addItem(rubber_band_);
        }
        rubber_band_->setLine(QLineF(press_pos_, press_pos_));
        rubber_band_->show();
    }
}

void DrawingChartView::mouseMoveEvent(QMouseEvent* event) {
    QPointF chartPos = chart()->mapToValue(event->position());
    if (crosshair_enabled_) {
        emit mouseMoved(chartPos, event->position());
    }
    if (mode_ != Mode::Placing || !is_placing_) {
        QChartView::mouseMoveEvent(event);
        return;
    }
    current_pos_ = event->position();
    if (rubber_band_ && active_tool_ != "Elliott Wave" && active_tool_ != "Text") {
        rubber_band_->setLine(QLineF(press_pos_, current_pos_));
    }
}

void DrawingChartView::mouseReleaseEvent(QMouseEvent* event) {
    if (active_tool_ == "Brush") {
        emit drawingFinished();
        return;
    }
    if (mode_ != Mode::Placing || !is_placing_) {
        QChartView::mouseReleaseEvent(event);
        return;
    }
    is_placing_ = false;
    if (rubber_band_) rubber_band_->hide();
    if (active_tool_ == "Elliott Wave" || active_tool_ == "Text") {
        emit drawingFinished();
    } else {
        QPointF start_scene = chart()->mapToValue(press_pos_);
        QPointF end_scene = chart()->mapToValue(event->position());
        emit objectPlaced(start_scene, end_scene);
        emit drawingFinished();
    }
}

void DrawingChartView::paintEvent(QPaintEvent* event) {
    QChartView::paintEvent(event);
}

// ═══════════════════════════════════════════════════════════════
// MT5FleetChartPanel
// ═══════════════════════════════════════════════════════════════

MT5FleetChartPanel::MT5FleetChartPanel(QWidget* parent) : QWidget(parent) {
    alert_manager_ = new ChartAlertManager(this);
    replay_engine_ = new BarReplayEngine(this);
    session_manager_ = new TimeSessionManager(this);
    econ_overlay_ = new EconomicEventOverlay(this);
    order_engine_ = new trading::SmartOrderEngine(this);

    connect(replay_engine_, &BarReplayEngine::barChanged, this, &MT5FleetChartPanel::on_replay_bar_changed);
    connect(alert_manager_, &ChartAlertManager::alertTriggered, this, &MT5FleetChartPanel::on_alert_triggered);

    // Low-latency execution gateway
    order_engine_->connectToGateway(mt5_ws_url(QStringLiteral("/ws/orders")));
    connect(order_engine_, &trading::SmartOrderEngine::orderFilled, this,
        [this](int ticket, const QString& symbol, const QString& side,
               double volume, double fillPrice, double latencyMs) {
            tool_label_->setText(QString("FILLED %1 %2 @ $%3 [%4ms]")
                .arg(side).arg(volume, 0, 'f', 2).arg(fillPrice, 0, 'f', 2).arg(latencyMs, 0, 'f', 1));
            refresh_positions();
            QTimer::singleShot(3000, this, [this](){ tool_label_->setText(""); });
        });
    connect(order_engine_, &trading::SmartOrderEngine::orderRejected, this,
        [this](const QString& reason) {
            tool_label_->setText("Order rejected: " + reason);
            QTimer::singleShot(3000, this, [this](){ tool_label_->setText(""); });
        });

    auto* pos_timer = new QTimer(this);
    connect(pos_timer, &QTimer::timeout, this, &MT5FleetChartPanel::refresh_positions);
    pos_timer->start(5000);

    build_ui(); apply_theme();
    setup_shortcuts();
    load_chart_data();
}

MT5FleetChartPanel::~MT5FleetChartPanel() {
    qDeleteAll(drawing_objects_);
    drawing_objects_.clear();
}

void MT5FleetChartPanel::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    // Header
    auto* header = new QWidget(this); header->setObjectName("chartHeader"); header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header); hl->setContentsMargins(10,0,10,0);
    symbol_label_ = new QLabel("XAUUSD", header); symbol_label_->setObjectName("chartSymbol");
    price_label_ = new QLabel("$0.00", header); price_label_->setObjectName("chartPrice");
    data_tooltip_label_ = new QLabel("", header); data_tooltip_label_->setObjectName("chartTool");
    tool_label_ = new QLabel("", header); tool_label_->setObjectName("chartTool");

    chart_type_combo_ = new QComboBox(header); chart_type_combo_->setObjectName("chartTimeframeCombo");
    chart_type_combo_->addItems(ChartTypeTransform::availableTransforms());
    connect(chart_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetChartPanel::on_chart_type_changed);

    chart_style_combo_ = new QComboBox(header); chart_style_combo_->setObjectName("chartTimeframeCombo");
    chart_style_combo_->addItems({"Candlestick","Bar","Line","Area"});
    connect(chart_style_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        set_chart_style(static_cast<ChartStyle>(idx));
    });

    timeframe_combo_ = new QComboBox(header); timeframe_combo_->setObjectName("chartTimeframeCombo");
    QStringList tfs = {"M1","M5","M15","M30","H1","H4","D1","W1","MN1"};
    for (auto& tf : tfs) timeframe_combo_->addItem(tf);
    timeframe_combo_->setCurrentText("H1");
    connect(timeframe_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetChartPanel::on_timeframe_changed);

    indicator_combo_ = new QComboBox(header); indicator_combo_->setObjectName("chartIndicatorCombo");
    indicator_combo_->addItems({"None","EMA9","EMA21","SMA20","SMA50","Bollinger","RSI","MACD","Stochastic","ADX"});
    connect(indicator_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetChartPanel::on_indicator_changed);

    compare_combo_ = new QComboBox(header); compare_combo_->setObjectName("chartTimeframeCombo");
    compare_combo_->addItem("Compare —");
    compare_combo_->addItems({"DXY","SPY","QQQ","BTCUSD","EURUSD","XAGUSD","VIX","US10Y"});
    connect(compare_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MT5FleetChartPanel::on_compare_symbol_changed);

    log_scale_check_ = new QCheckBox("Log", header); log_scale_check_->setObjectName("chartToolBtn");
    log_scale_check_->setFixedHeight(22);
    connect(log_scale_check_, &QCheckBox::toggled, this, &MT5FleetChartPanel::on_log_scale_toggled);

    refresh_btn_ = new QPushButton("Refresh", header); refresh_btn_->setObjectName("chartToolBtn");
    connect(refresh_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::refresh_chart);

    hl->addWidget(symbol_label_); hl->addSpacing(4);
    hl->addWidget(price_label_); hl->addSpacing(4);
    hl->addWidget(data_tooltip_label_); hl->addSpacing(4);
    hl->addWidget(tool_label_); hl->addSpacing(4);
    hl->addStretch();
    hl->addWidget(compare_combo_);
    hl->addWidget(log_scale_check_);
    hl->addWidget(chart_type_combo_);
    hl->addWidget(chart_style_combo_); hl->addWidget(timeframe_combo_); hl->addWidget(indicator_combo_); hl->addWidget(refresh_btn_);
    root->addWidget(header);

    // TradingView-style drawing toolbar. It is inserted into the chart area
    // below so the tools sit on the chart's left edge instead of above it.
    drawing_toolbar_ = new QWidget(this);
    drawing_toolbar_->setObjectName("chartDrawToolRail");
    drawing_toolbar_->setFixedWidth(44);
    auto* dl = new QVBoxLayout(drawing_toolbar_);
    dl->setContentsMargins(5, 6, 5, 6);
    dl->setSpacing(4);

    auto addDrawTool = [&](const QString& tool, const QString& label, QPushButton** slot = nullptr) {
        auto* btn = new QPushButton(label, drawing_toolbar_);
        btn->setObjectName("chartRailToolBtn");
        btn->setProperty("toolName", tool);
        btn->setFixedSize(34, 26);
        btn->setCheckable(true);
        btn->setToolTip(tool);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, &MT5FleetChartPanel::on_drawing_tool_clicked);
        dl->addWidget(btn);
        drawing_tool_buttons_.append(btn);
        if (slot) *slot = btn;
        return btn;
    };

    cursor_btn_ = addDrawTool("None", "⌖");
    cursor_btn_->setToolTip("Cursor / select");
    cursor_btn_->setChecked(true);
    trendline_btn_ = addDrawTool("TrendLine", "╱");
    addDrawTool("Ray", "⟋");
    hline_btn_ = addDrawTool("H-Line", "─");
    vline_btn_ = addDrawTool("V-Line", "│");
    channel_btn_ = addDrawTool("Channel", "▱");
    addDrawTool("Pitchfork", "Ψ");
    fib_btn_ = addDrawTool("Fib Retrace", "Fib");
    fib_ext_btn_ = addDrawTool("Fib Ext", "Ext");
    fib_fan_btn_ = addDrawTool("Fib Fan", "Fan");
    addDrawTool("Fib Arc", "Arc");
    gann_fan_btn_ = addDrawTool("Gann Fan", "G");
    addDrawTool("Gann Square", "□");
    addDrawTool("Cycle Line", "◷");
    elliott_btn_ = addDrawTool("Elliott Wave", "EW");
    addDrawTool("Brush", "✎");
    addDrawTool("Measure", "⟷");
    text_label_btn_ = addDrawTool("Text", "T");

    dl->addSpacing(4);
    clear_draw_btn_ = new QPushButton("×", drawing_toolbar_); clear_draw_btn_->setObjectName("chartRailToolBtn");
    clear_draw_btn_->setFixedSize(34, 26);
    clear_draw_btn_->setToolTip("Clear drawings");
    connect(clear_draw_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::clear_drawing_tools);
    dl->addWidget(clear_draw_btn_);
    undo_draw_btn_ = new QPushButton("↶", drawing_toolbar_); undo_draw_btn_->setObjectName("chartRailToolBtn");
    undo_draw_btn_->setFixedSize(34, 26);
    undo_draw_btn_->setToolTip("Undo last drawing");
    connect(undo_draw_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::undo_last_tool);
    dl->addWidget(undo_draw_btn_);
    save_draw_btn_ = new QPushButton("⬇", drawing_toolbar_); save_draw_btn_->setObjectName("chartRailToolBtn");
    save_draw_btn_->setFixedSize(34, 26);
    save_draw_btn_->setToolTip("Save drawings");
    connect(save_draw_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::save_drawings);
    dl->addWidget(save_draw_btn_);
    load_draw_btn_ = new QPushButton("⬆", drawing_toolbar_); load_draw_btn_->setObjectName("chartRailToolBtn");
    load_draw_btn_->setFixedSize(34, 26);
    load_draw_btn_->setToolTip("Load drawings");
    connect(load_draw_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::load_drawings);
    dl->addWidget(load_draw_btn_);

    magnet_btn_ = new QPushButton("Mag", drawing_toolbar_); magnet_btn_->setObjectName("chartRailToolBtn");
    magnet_btn_->setFixedSize(34, 26);
    magnet_btn_->setCheckable(true);
    magnet_btn_->setToolTip("Magnet snap to candle OHLC");
    connect(magnet_btn_, &QPushButton::toggled, this, [this](bool checked) {
        magnet_enabled_ = checked;
        tool_label_->setText(checked ? "Magnet on" : "Magnet off");
        QTimer::singleShot(1600, this, [this](){ tool_label_->setText(""); });
    });
    dl->addWidget(magnet_btn_);

    lock_draw_btn_ = new QPushButton("Lock", drawing_toolbar_); lock_draw_btn_->setObjectName("chartRailToolBtn");
    lock_draw_btn_->setFixedSize(34, 26);
    lock_draw_btn_->setCheckable(true);
    lock_draw_btn_->setToolTip("Lock drawings");
    connect(lock_draw_btn_, &QPushButton::toggled, this, [this](bool checked) {
        drawings_locked_ = checked;
        for (auto* draw : drawing_objects_) draw->setLocked(checked);
        tool_label_->setText(checked ? "Drawings locked" : "Drawings unlocked");
        QTimer::singleShot(1600, this, [this](){ tool_label_->setText(""); });
    });
    dl->addWidget(lock_draw_btn_);

    hide_draw_btn_ = new QPushButton("Hide", drawing_toolbar_); hide_draw_btn_->setObjectName("chartRailToolBtn");
    hide_draw_btn_->setFixedSize(34, 26);
    hide_draw_btn_->setCheckable(true);
    hide_draw_btn_->setToolTip("Hide drawings");
    connect(hide_draw_btn_, &QPushButton::toggled, this, [this](bool checked) {
        drawings_hidden_ = checked;
        for (auto* draw : drawing_objects_) draw->setVisible(!checked);
        tool_label_->setText(checked ? "Drawings hidden" : "Drawings shown");
        QTimer::singleShot(1600, this, [this](){ tool_label_->setText(""); });
    });
    dl->addWidget(hide_draw_btn_);

    object_tree_btn_ = new QPushButton("Obj", drawing_toolbar_); object_tree_btn_->setObjectName("chartRailToolBtn");
    object_tree_btn_->setFixedSize(34, 26);
    object_tree_btn_->setToolTip("Object tree");
    connect(object_tree_btn_, &QPushButton::clicked, this, [this]() {
        QStringList rows;
        for (int i = 0; i < drawing_objects_.size(); ++i) {
            auto* draw = drawing_objects_[i];
            rows << QString("%1. %2  %3%4")
                .arg(i + 1)
                .arg(draw->toolName())
                .arg(draw->isLocked() ? "[locked] " : "")
                .arg(draw->isVisible() ? "" : "[hidden]");
        }
        QMessageBox::information(this, "Object Tree", rows.isEmpty() ? "No drawings" : rows.join("\n"));
    });
    dl->addWidget(object_tree_btn_);

    drawing_style_btn_ = new QPushButton("Sty", drawing_toolbar_); drawing_style_btn_->setObjectName("chartRailToolBtn");
    drawing_style_btn_->setFixedSize(34, 26);
    drawing_style_btn_->setToolTip("Drawing style");
    connect(drawing_style_btn_, &QPushButton::clicked, this, [this]() {
        QVector<DrawingObject*> targets;
        for (auto* item : chart_->scene()->selectedItems()) {
            if (auto* draw = qgraphicsitem_cast<DrawingObject*>(item)) targets.append(draw);
        }
        if (targets.isEmpty()) targets = drawing_objects_;
        if (targets.isEmpty()) {
            tool_label_->setText("No drawings to style");
            QTimer::singleShot(1600, this, [this](){ tool_label_->setText(""); });
            return;
        }

        QColor initial = targets.first()->color();
        QColor color = QColorDialog::getColor(initial, this, "Drawing Color");
        if (!color.isValid()) return;
        bool ok = false;
        double width = QInputDialog::getDouble(this, "Line Width", "Width:", targets.first()->lineWidth(), 0.5, 12.0, 1, &ok);
        if (!ok) return;
        for (auto* draw : targets) {
            draw->setColor(color);
            draw->setLineWidth(width);
        }
        tool_label_->setText(QString("Styled %1 drawing(s)").arg(targets.size()));
        QTimer::singleShot(1600, this, [this](){ tool_label_->setText(""); });
    });
    dl->addWidget(drawing_style_btn_);
    dl->addStretch();

    // Features toolbar
    auto* feat_bar = new QWidget(this); feat_bar->setObjectName("chartDrawBar"); feat_bar->setFixedHeight(28);
    auto* fl = new QHBoxLayout(feat_bar); fl->setContentsMargins(8,0,8,0); fl->setSpacing(3);

    crosshair_btn_ = new QPushButton("Crosshair", feat_bar); crosshair_btn_->setObjectName("chartToolBtn");
    crosshair_btn_->setFixedHeight(20); crosshair_btn_->setCheckable(true);
    connect(crosshair_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_crosshair_toggled);
    fl->addWidget(crosshair_btn_);

    fullscreen_btn_ = new QPushButton("Fullscreen", feat_bar); fullscreen_btn_->setObjectName("chartToolBtn");
    fullscreen_btn_->setFixedHeight(20);
    connect(fullscreen_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_fullscreen_toggled);
    fl->addWidget(fullscreen_btn_);

    screenshot_btn_ = new QPushButton("Screenshot", feat_bar); screenshot_btn_->setObjectName("chartToolBtn");
    screenshot_btn_->setFixedHeight(20);
    connect(screenshot_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::take_screenshot);
    fl->addWidget(screenshot_btn_);

    vp_btn_ = new QPushButton("VP", feat_bar); vp_btn_->setObjectName("chartToolBtn");
    vp_btn_->setFixedHeight(20); vp_btn_->setCheckable(true);
    connect(vp_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_vp_toggled);
    fl->addWidget(vp_btn_);

    session_btn_ = new QPushButton("Sessions", feat_bar); session_btn_->setObjectName("chartToolBtn");
    session_btn_->setFixedHeight(20); session_btn_->setCheckable(true);
    connect(session_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_session_toggled);
    fl->addWidget(session_btn_);

    econ_btn_ = new QPushButton("Events", feat_bar); econ_btn_->setObjectName("chartToolBtn");
    econ_btn_->setFixedHeight(20); econ_btn_->setCheckable(true);
    connect(econ_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_econ_toggled);
    fl->addWidget(econ_btn_);

    auto* polygon_btn = new QPushButton("Polygon", feat_bar); polygon_btn->setObjectName("chartToolBtn");
    polygon_btn->setFixedHeight(20); polygon_btn->setCheckable(true);
    connect(polygon_btn, &QPushButton::toggled, this, [this](bool checked) {
        if (!polygon_overlay_) {
            polygon_overlay_ = new PolygonOverlay(this, this);
            chart_col_->addWidget(polygon_overlay_);
        }
        polygon_overlay_->setVisible(checked);
    });
    fl->addWidget(polygon_btn);

    auto* exec_btn = new QPushButton("Execute", feat_bar); exec_btn->setObjectName("chartToolBtn");
    exec_btn->setFixedHeight(20); exec_btn->setCheckable(true);
    connect(exec_btn, &QPushButton::toggled, this, [this](bool checked) {
        if (!exec_panel_) {
            exec_panel_ = new ExecutionPanel(this, chart_container_);
            chart_wrap_->addWidget(exec_panel_);
        }
        exec_panel_->setVisible(checked);
        if (checked) exec_panel_->raise();
    });
    fl->addWidget(exec_btn);

    alert_btn_ = new QPushButton("Alert", feat_bar); alert_btn_->setObjectName("chartToolBtn");
    alert_btn_->setFixedHeight(20);
    connect(alert_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_add_alert);
    fl->addWidget(alert_btn_);

    bar_coloring_btn_ = new QPushButton("Color Bars", feat_bar); bar_coloring_btn_->setObjectName("chartToolBtn");
    bar_coloring_btn_->setFixedHeight(20); bar_coloring_btn_->setCheckable(true);
    connect(bar_coloring_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_bar_coloring_toggled);
    fl->addWidget(bar_coloring_btn_);

    paper_trade_btn_ = new QPushButton("Paper Trade", feat_bar); paper_trade_btn_->setObjectName("chartToolBtn");
    paper_trade_btn_->setFixedHeight(20);
    connect(paper_trade_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_paper_trade);
    fl->addWidget(paper_trade_btn_);
    paper_strategy_combo_ = new QComboBox(feat_bar);
    paper_strategy_combo_->addItems({"EMA Crossover", "SMA Crossover", "RSI Strategy"});
    paper_strategy_combo_->setFixedHeight(20);
    paper_strategy_combo_->setStyleSheet("background:#1a1a2e;color:#e5e5e5;border:1px solid #2a2a3e;");
    fl->addWidget(paper_strategy_combo_);

    indicator_params_btn_ = new QPushButton("Params", feat_bar); indicator_params_btn_->setObjectName("chartToolBtn");
    indicator_params_btn_->setFixedHeight(20);
    connect(indicator_params_btn_, &QPushButton::clicked, this, [this](){ edit_indicator_params(); });
    fl->addWidget(indicator_params_btn_);

    replay_toggle_btn_ = new QPushButton("Replay", feat_bar); replay_toggle_btn_->setObjectName("chartToolBtn");
    replay_toggle_btn_->setFixedHeight(20);
    connect(replay_toggle_btn_, &QPushButton::clicked, this, [this]() {
        replay_bar_->setVisible(!replay_bar_->isVisible());
        if (replay_bar_->isVisible() && replay_engine_->totalBars() == 0) replay_engine_->loadData(chart_data_);
    });
    fl->addWidget(replay_toggle_btn_);

    auto* compiler_status_btn = new QPushButton("Compiler", feat_bar); compiler_status_btn->setObjectName("chartToolBtn");
    compiler_status_btn->setFixedHeight(20);
    connect(compiler_status_btn, &QPushButton::clicked, this, [this]() {
        tool_label_->setText("Checking compiler...");
        HttpClient::instance().get("/mt5/compiler-status",
            [this](Result<QJsonDocument> r) {
                if (r.is_ok()) {
                    auto d = r.value().object()["data"].toObject();
                    bool ok = d["can_compile"].toBool();
                    bool wine = d["wine_available"].toBool();
                    bool dev = d["dev_mode"].toBool();
                    QString msg = ok ? (dev ? "Dev mode (simulated)" : "Compiler ready")
                                     : (wine ? "MT5 not installed. Click Setup" : "Install Wine first");
                    tool_label_->setText("Compiler: " + msg);
                } else {
                    tool_label_->setText("Backend not running");
                }
                QTimer::singleShot(5000, this, [this](){ tool_label_->setText(""); });
            }, this);
    });
    fl->addWidget(compiler_status_btn);

    save_template_btn_ = new QPushButton("Save Tmpl", feat_bar); save_template_btn_->setObjectName("chartToolBtn");
    save_template_btn_->setFixedHeight(20);
    connect(save_template_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_chart_template_save);
    fl->addWidget(save_template_btn_);

    load_template_btn_ = new QPushButton("Load Tmpl", feat_bar); load_template_btn_->setObjectName("chartToolBtn");
    load_template_btn_->setFixedHeight(20);
    connect(load_template_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_chart_template_load);
    fl->addWidget(load_template_btn_);

    fl->addStretch();
    fl->addWidget(new QLabel("From:", feat_bar));
    date_from_ = new QDateEdit(QDate::currentDate().addMonths(-3), feat_bar); date_from_->setObjectName("chartTimeframeCombo");
    date_from_->setCalendarPopup(true); date_from_->setFixedHeight(20);
    fl->addWidget(date_from_);
    fl->addWidget(new QLabel("To:", feat_bar));
    date_to_ = new QDateEdit(QDate::currentDate(), feat_bar); date_to_->setObjectName("chartTimeframeCombo");
    date_to_->setCalendarPopup(true); date_to_->setFixedHeight(20);
    fl->addWidget(date_to_);
    date_apply_btn_ = new QPushButton("Go", feat_bar); date_apply_btn_->setObjectName("chartToolBtn");
    date_apply_btn_->setFixedHeight(20);
    connect(date_apply_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_date_range_apply);
    fl->addWidget(date_apply_btn_);

    root->addWidget(feat_bar);

    // Replay bar (hidden initially)
    replay_bar_ = new QWidget(this); replay_bar_->setObjectName("chartDrawBar"); replay_bar_->setFixedHeight(28);
    replay_bar_->hide();
    auto* rl = new QHBoxLayout(replay_bar_); rl->setContentsMargins(8,0,8,0); rl->setSpacing(3);
    replay_step_back_btn_ = new QPushButton("|<", replay_bar_); replay_step_back_btn_->setObjectName("chartToolBtn");
    replay_step_back_btn_->setFixedHeight(20);
    connect(replay_step_back_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_replay_step_back);
    rl->addWidget(replay_step_back_btn_);
    replay_play_btn_ = new QPushButton("Play", replay_bar_); replay_play_btn_->setObjectName("chartToolBtn");
    replay_play_btn_->setFixedHeight(20);
    connect(replay_play_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_replay_play);
    rl->addWidget(replay_play_btn_);
    replay_stop_btn_ = new QPushButton("Stop", replay_bar_); replay_stop_btn_->setObjectName("chartToolBtn");
    replay_stop_btn_->setFixedHeight(20);
    connect(replay_stop_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_replay_stop);
    rl->addWidget(replay_stop_btn_);
    replay_step_fwd_btn_ = new QPushButton(">|", replay_bar_); replay_step_fwd_btn_->setObjectName("chartToolBtn");
    replay_step_fwd_btn_->setFixedHeight(20);
    connect(replay_step_fwd_btn_, &QPushButton::clicked, this, &MT5FleetChartPanel::on_replay_step_fwd);
    rl->addWidget(replay_step_fwd_btn_);
    replay_label_ = new QLabel("0 / 0", replay_bar_); replay_label_->setObjectName("chartTool");
    rl->addWidget(replay_label_);
    rl->addStretch();
    root->addWidget(replay_bar_);

    // Chart container (holds main chart + indicator pane + volume profile)
    chart_container_ = new QWidget(this);
    chart_wrap_ = new QHBoxLayout(chart_container_);
    chart_wrap_->setContentsMargins(0, 0, 0, 0);
    chart_wrap_->setSpacing(0);

    chart_col_ = new QVBoxLayout();
    chart_col_->setContentsMargins(0, 0, 0, 0);
    chart_col_->setSpacing(1);

    // CryptoChart — full TradingView-like chart with crosshair, OHLC tooltip,
    // price/time tags, timeframe buttons, wheel zoom, and drag pan.
    crypto_chart_ = new crypto::CryptoChart(chart_container_);
    chart_ = crypto_chart_->chart();
    chart_view_ = qobject_cast<QChartView*>(crypto_chart_->chartView());

    // Event filter for drawing tools + context menu
    chart_view_->installEventFilter(this);
    chart_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(chart_view_, &QWidget::customContextMenuRequested, this, &MT5FleetChartPanel::show_chart_context_menu);

    chart_col_->addWidget(crypto_chart_, 1);

    indicator_pane_ = new IndicatorPane(chart_container_);
    chart_col_->addWidget(indicator_pane_);

    chart_wrap_->addWidget(drawing_toolbar_);
    chart_wrap_->addLayout(chart_col_, 1);

    volume_profile_ = new VolumeProfileLayer(chart_container_);
    volume_profile_->setFixedWidth(80);
    volume_profile_->hide();
    chart_wrap_->addWidget(volume_profile_);

    root->addWidget(chart_container_, 1);

    // Context menu — live trading
    context_menu_ = new QMenu(this);
    auto placeOrder = [this](const QString& side) {
        bool ok;
        double volume = QInputDialog::getDouble(this, side + " XAUUSD", "Volume (lots):", 0.01, 0.01, 100, 2, &ok);
        if (!ok) return;
        double sl = QInputDialog::getDouble(this, "Stop Loss", "SL ($ away from entry):", 10, 0, 10000, 2, &ok);
        if (!ok) return;
        double tp = QInputDialog::getDouble(this, "Take Profit", "TP ($ away from entry):", 20, 0, 100000, 2, &ok);
        if (!ok) return;

        QJsonObject body;
        body["symbol"] = current_symbol_;
        body["side"] = side;
        body["volume"] = volume;
        body["sl"] = sl;
        body["tp"] = tp;
        body["ea_key"] = "";

        tool_label_->setText(QString("Placing %1 %2 (low-latency)...").arg(side).arg(volume, 0, 'f', 2));
        if (order_engine_->submitOrder(current_symbol_, side, volume, sl, tp)) {
            tool_label_->setText(QString("%1 %2 queued ✓").arg(side).arg(volume, 0, 'f', 2));
        } else {
            tool_label_->setText("Order engine queue full — try again");
        }
        QTimer::singleShot(3000, this, [this](){ tool_label_->setText(""); });
    };

    context_menu_->addAction("Buy Market", this, [placeOrder](){ placeOrder("BUY"); });
    context_menu_->addAction("Sell Market", this, [placeOrder](){ placeOrder("SELL"); });
    context_menu_->addSeparator();
    context_menu_->addAction("Set Alert", this, &MT5FleetChartPanel::on_add_alert);
    context_menu_->addAction("Copy Price", this, [this](){
        if (!ohlc_data_.isEmpty()) QApplication::clipboard()->setText(QString::number(ohlc_data_.last().close));
    });
    context_menu_->addSeparator();
    context_menu_->addAction("Refresh Positions", this, &MT5FleetChartPanel::refresh_positions);
    context_menu_->addAction("Add to Watchlist", this, [this](){ tool_label_->setText("Added to watchlist"); });
}

void MT5FleetChartPanel::setup_shortcuts() {
    new QShortcut(QKeySequence("F11"), this, this, &MT5FleetChartPanel::on_fullscreen_toggled);
    new QShortcut(QKeySequence("Ctrl+S"), this, this, &MT5FleetChartPanel::save_drawings);
    new QShortcut(QKeySequence("Ctrl+O"), this, this, &MT5FleetChartPanel::load_drawings);
    new QShortcut(QKeySequence("R"), this, this, &MT5FleetChartPanel::refresh_chart);
    new QShortcut(QKeySequence("Space"), this, this, &MT5FleetChartPanel::on_replay_play);
}

void MT5FleetChartPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#chartHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#chartSymbol{color:%3;font-size:16px;font-weight:700;}"
        "QLabel#chartPrice{color:%4;font-size:14px;font-weight:600;}"
        "QLabel#chartTool{color:%5;font-size:11px;background:%6;padding:0 6px;border:1px solid %2;}"
        "QWidget#chartDrawBar{background:%1;border-bottom:1px solid %2;}"
        "QWidget#chartDrawToolRail{background:%1;border-right:1px solid %2;}"
        "QComboBox#chartTimeframeCombo,QComboBox#chartIndicatorCombo{background:%6;color:%3;border:1px solid %2;padding:3px 6px;}"
        "QPushButton#chartToolBtn,QCheckBox#chartToolBtn{background:%6;color:%3;border:1px solid %2;padding:3px 6px;font-size:10px;max-height:22px;}"
        "QPushButton#chartToolBtn:hover,QCheckBox#chartToolBtn:hover{background:%7;}"
        "QPushButton#chartToolBtn:checked{background:%4;color:#FFF;}"
        "QPushButton#chartRailToolBtn{background:%6;color:%3;border:1px solid transparent;padding:0;font-size:10px;font-weight:700;}"
        "QPushButton#chartRailToolBtn:hover{background:%7;border-color:%2;}"
        "QPushButton#chartRailToolBtn:checked{background:%4;color:#FFF;border-color:%4;}"
        "QChartView#chartView{background:%8;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
          ui::colors::AMBER(), ui::colors::TEXT_SECONDARY(),
          ui::colors::BG_RAISED(), ui::colors::BG_HOVER(), ui::colors::BG_BASE()));
}

void MT5FleetChartPanel::set_symbol(const QString& symbol) {
    current_symbol_ = symbol; symbol_label_->setText(symbol);
    load_chart_data();
}

void MT5FleetChartPanel::set_timeframe(const QString& tf) {
    int idx = timeframe_combo_->findText(tf); if(idx>=0) timeframe_combo_->setCurrentIndex(idx);
}

void MT5FleetChartPanel::set_order_markers(const QVector<OrderMarker>& markers) {
    order_markers_ = markers;
    if (!ohlc_data_.isEmpty()) render_order_markers();
}

void MT5FleetChartPanel::set_chart_style(ChartStyle style) {
    chart_style_ = style;
    if (!ohlc_data_.isEmpty()) load_chart_data();
}

void MT5FleetChartPanel::load_chart_data() {
    QString url = mt5_ohlc_path(current_symbol_, current_timeframe_, MAX_POINTS);

    HttpClient::instance().get(url,
        [this](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            if (arr.isEmpty()) return;

            ohlc_data_.clear();
            chart_data_.clear();
            transformed_data_.clear();
            crypto_chart_->clear();
            qDeleteAll(indicator_series_); indicator_series_.clear();
            qDeleteAll(order_marker_series_); order_marker_series_.clear();
            for (auto* t : order_text_items_) { chart_->scene()->removeItem(t); delete t; }
            order_text_items_.clear();
            for (auto* r : time_session_rects_) { chart_->scene()->removeItem(r); delete r; }
            time_session_rects_.clear();
            for (auto* e : econ_event_items_) { chart_->scene()->removeItem(e); delete e; }
            econ_event_items_.clear();
            for (auto* a : alert_marker_items_) { chart_->scene()->removeItem(a); delete a; }
            alert_marker_items_.clear();
            axis_x_ = nullptr; axis_y_ = nullptr; volume_axis_ = nullptr;

            qreal min_p = 1e9, max_p = 0;
            qreal min_t = 0, max_t = 0;
            qreal max_vol = 0;

            auto& src = ohlc_data_;
            for (auto v : arr) {
                auto obj = v.toObject();
                OhlcvPoint pt;
                pt.time = obj["time"].toDouble() * 1000;
                pt.open = obj["open"].toDouble();
                pt.high = obj["high"].toDouble();
                pt.low = obj["low"].toDouble();
                pt.close = obj["close"].toDouble();
                pt.volume = obj["tick_volume"].toDouble();
                src.append(pt);
                chart_data_.append(QPointF(pt.time, pt.close));
                min_p = qMin(min_p, pt.low);
                max_p = qMax(max_p, pt.high);
                max_vol = qMax(max_vol, pt.volume);
                if (min_t == 0) min_t = pt.time;
                max_t = pt.time;
            }

            if (src.isEmpty()) return;

            // Apply chart type transform
            apply_chart_type_transform();
            const auto& display_data = transformed_data_.isEmpty() ? src : transformed_data_;

            // Push data to CryptoChart — full TradingView-like rendering, crosshair,
            // OHLC tooltip, price/time tags, wheel zoom, drag pan.
            if (crypto_chart_) {
                QVector<trading::Candle> candles;
                candles.reserve(display_data.size());
                for (const auto& pt : display_data) {
                    trading::Candle c;
                    c.timestamp = static_cast<int64_t>(pt.time);
                    c.open = pt.open; c.high = pt.high;
                    c.low = pt.low; c.close = pt.close; c.volume = pt.volume;
                    candles.append(c);
                }
                crypto_chart_->set_candles(candles);
                axis_x_ = qobject_cast<QDateTimeAxis*>(crypto_chart_->chart()->axisX());
                axis_y_ = crypto_chart_->chart()->axisY();
            }

            render_volume_bars();
            render_price_line();
            update_last_price_marker();

            if (!order_markers_.isEmpty()) render_order_markers();

            double last_close = ohlc_data_.last().close;
            price_label_->setText(QString("$%1").arg(last_close, 0, 'f', 2));

            if (!current_compare_symbol_.isEmpty()) load_compare_data(current_compare_symbol_);

            // Re-apply indicator
            QString ind_name = indicator_combo_->currentText();
            if (ind_name != "None") {
                if (is_oscillator(ind_name)) add_oscillator_indicator(ind_name);
                else add_indicator(ind_name);
            }

            // Re-apply optional overlays
            if (session_enabled_) render_time_sessions();
            if (econ_enabled_) render_econ_events();
            if (vp_enabled_ && !ohlc_data_.isEmpty()) {
                QVector<qreal> prices, vols;
                for (const auto& pt : ohlc_data_) { prices.append(pt.close); vols.append(pt.volume); }
                volume_profile_->setData(prices, vols);
            }
            if (alert_manager_->activeAlerts().size() > 0) render_alerts();

            // Load replay engine
            replay_engine_->loadData(chart_data_);
        }, this);
}

void MT5FleetChartPanel::apply_chart_type_transform() {
    if (current_chart_type_ == "None" || current_chart_type_.isEmpty()) {
        transformed_data_.clear();
        return;
    }
    transformed_data_ = ChartTypeTransform::transform(current_chart_type_, ohlc_data_);
}

void MT5FleetChartPanel::render_volume_bars() {
    if (ohlc_data_.isEmpty()) return;

    volume_up_set_ = new QBarSet("Vol Up");
    volume_down_set_ = new QBarSet("Vol Down");
    volume_up_set_->setColor(QColor(ui::colors::POSITIVE()).lighter(130));
    volume_down_set_->setColor(QColor(ui::colors::NEGATIVE()).lighter(130));

    qreal max_vol = 0;
    QStringList categories;
    for (int i = 0; i < ohlc_data_.size(); ++i) {
        const auto& pt = ohlc_data_[i];
        max_vol = qMax(max_vol, pt.volume);
        bool is_up = pt.close >= pt.open;
        *volume_up_set_ << (is_up ? pt.volume : 0);
        *volume_down_set_ << (is_up ? 0 : pt.volume);
        categories << QString::number(i);
    }

    volume_bars_ = new QBarSeries();
    volume_bars_->append(volume_up_set_);
    volume_bars_->append(volume_down_set_);
    volume_bars_->setBarWidth(0.8);

    auto* cat_axis = new QBarCategoryAxis();
    cat_axis->hide();
    chart_->addAxis(cat_axis, Qt::AlignBottom);

    volume_axis_ = new QValueAxis();
    volume_axis_->setRange(0, max_vol * 3.5);
    volume_axis_->setLabelsVisible(false);
    volume_axis_->setGridLineVisible(false);
    volume_axis_->setLineVisible(false);
    chart_->addAxis(volume_axis_, Qt::AlignLeft);

    chart_->addSeries(volume_bars_);
    volume_bars_->attachAxis(cat_axis);
    volume_bars_->attachAxis(volume_axis_);

    volume_bars_->setOpacity(0.35);
}

void MT5FleetChartPanel::render_price_line() {
    if (ohlc_data_.isEmpty()) return;
    double last_close = ohlc_data_.last().close;

    price_line_series_ = new QLineSeries();
    price_line_series_->setPen(QPen(QColor(ui::colors::AMBER()), 1, Qt::DashLine));
    qint64 min_t = static_cast<qint64>(ohlc_data_.first().time);
    qint64 max_t = static_cast<qint64>(ohlc_data_.last().time);
    price_line_series_->append(min_t, last_close);
    price_line_series_->append(max_t, last_close);
    chart_->addSeries(price_line_series_);
    if (axis_x_) price_line_series_->attachAxis(axis_x_);
    if (axis_y_) price_line_series_->attachAxis(axis_y_);
}

void MT5FleetChartPanel::render_order_markers() {
    auto axisMin = [](QAbstractAxis* a) -> qreal {
        if (auto* v = qobject_cast<QValueAxis*>(a)) return v->min();
        if (auto* l = qobject_cast<QLogValueAxis*>(a)) return l->min();
        return 0;
    };
    auto axisMax = [](QAbstractAxis* a) -> qreal {
        if (auto* v = qobject_cast<QValueAxis*>(a)) return v->max();
        if (auto* l = qobject_cast<QLogValueAxis*>(a)) return l->max();
        return 1;
    };
    qreal y_bottom = axis_y_ ? axisMin(axis_y_) : 0;
    qreal y_top = axis_y_ ? axisMax(axis_y_) : 1;

    for (const auto& m : order_markers_) {
        auto* line = new QLineSeries();
        QColor c = (m.side == "BUY") ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());
        line->setPen(QPen(c, 1.5, Qt::DotLine));
        line->append(static_cast<qreal>(m.time), y_bottom);
        line->append(static_cast<qreal>(m.time), y_top);
        chart_->addSeries(line);
        line->attachAxis(axis_x_);
        line->attachAxis(axis_y_);
        order_marker_series_.append(line);

        auto* text = new QGraphicsSimpleTextItem(
            QString("%1 %2\n%3 lot").arg(m.side).arg(m.price, 0, 'f', 2).arg(m.lot, 0, 'f', 2));
        text->setBrush(QBrush(c));
        text->setFont(QFont("Consolas", 9));
        chart_->scene()->addItem(text);
        order_text_items_.append(text);
    }
}

void MT5FleetChartPanel::render_time_sessions() {
    if (ohlc_data_.isEmpty() || !session_enabled_) return;
    qint64 first_ts = static_cast<qint64>(ohlc_data_.first().time);
    session_manager_->generateDailySessions(first_ts);
    auto sessions = session_manager_->sessions();
    for (const auto& s : sessions) {
        if (s.type == TimeSession::Regular) continue;
        auto* rect = new QGraphicsRectItem();
        QPointF topLeft(static_cast<qreal>(s.startTime), axis_y_ ? axisMax(axis_y_) : 0);
        QPointF bottomRight(static_cast<qreal>(s.endTime), axis_y_ ? axisMin(axis_y_) : 0);
        rect->setRect(QRectF(topLeft, bottomRight));
        rect->setBrush(QBrush(s.color));
        rect->setPen(QPen(Qt::NoPen));
        chart_->scene()->addItem(rect);
        time_session_rects_.append(rect);
    }
}

void MT5FleetChartPanel::render_econ_events() {
    if (ohlc_data_.isEmpty() || !econ_enabled_) return;
    for (auto* e : econ_event_items_) { chart_->scene()->removeItem(e); delete e; }
    econ_event_items_.clear();

    auto events = econ_overlay_->events();
    for (const auto& ev : events) {
        auto* text = new QGraphicsSimpleTextItem(ev.title.left(10));
        QColor c = ev.impact == "high" ? QColor(255,100,100) : ev.impact == "medium" ? QColor(255,200,100) : QColor(150,150,150);
        text->setBrush(QBrush(c));
        text->setFont(QFont("Consolas", 7));
        QPointF pos(static_cast<qreal>(ev.timestamp), axis_y_ ? axisMax(axis_y_) : 0);
        text->setPos(pos);
        chart_->scene()->addItem(text);
        econ_event_items_.append(text);
    }
}

void MT5FleetChartPanel::render_alerts() {
    for (auto* a : alert_marker_items_) { chart_->scene()->removeItem(a); delete a; }
    alert_marker_items_.clear();

    for (const auto& alert : alert_manager_->activeAlerts()) {
        auto* line = new QGraphicsLineItem();
        qreal y = alert.price;
        qreal x = static_cast<qreal>(alert.timestamp);
        line->setLine(x, axis_y_ ? axisMin(axis_y_) : 0, x, axis_y_ ? axisMax(axis_y_) : 0);
        line->setPen(QPen(QColor(alert.color), 1, Qt::DashDotLine));
        chart_->scene()->addItem(line);
        alert_marker_items_.append(line);

        auto* label = new QGraphicsSimpleTextItem(alert.label);
        label->setBrush(QBrush(alert.color));
        label->setFont(QFont("Consolas", 7));
        label->setPos(x, y);
        chart_->scene()->addItem(label);
        alert_marker_items_.append(label);
    }
}

void MT5FleetChartPanel::load_compare_data(const QString& compareSymbol) {
    if (ohlc_data_.isEmpty()) return;
    QString url = mt5_ohlc_path(compareSymbol, current_timeframe_, MAX_POINTS);

    HttpClient::instance().get(url,
        [this, compareSymbol](Result<QJsonDocument> r) {
            if (r.is_err()) return;
            auto arr = r.value().object()["data"].toArray();
            if (arr.isEmpty()) return;

            if (compare_series_) {
                chart_->removeSeries(compare_series_);
                delete compare_series_;
                compare_series_ = nullptr;
            }

            compare_series_ = new QLineSeries();
            compare_series_->setName(compareSymbol);
            compare_series_->setPen(QPen(QColor(255,100,255), 1.2, Qt::DashLine));

            double first_close = 0;
            QVector<QPointF> points;
            for (auto v : arr) {
                auto obj = v.toObject();
                qreal ts = obj["time"].toDouble() * 1000;
                qreal close = obj["close"].toDouble();
                if (first_close == 0) first_close = close;
                qreal pct = ((close - first_close) / first_close) * 100;
                points.append(QPointF(ts, pct));
            }

            double main_first = ohlc_data_.first().close;
            for (const auto& pt : points) {
                qreal scaled = main_first * (1 + pt.y() / 100);
                compare_series_->append(pt.x(), scaled);
            }

            chart_->addSeries(compare_series_);
            if (axis_x_) compare_series_->attachAxis(axis_x_);
            if (axis_y_) compare_series_->attachAxis(axis_y_);

            tool_label_->setText(QString("Overlay: %1").arg(compareSymbol));
            QTimer::singleShot(3000, this, [this](){ tool_label_->setText(""); });
        }, this);
}

void MT5FleetChartPanel::refresh_chart() { load_chart_data(); }

void MT5FleetChartPanel::on_timeframe_changed(int) {
    current_timeframe_ = timeframe_combo_->currentText();
    clear_drawing_tools();
    replay_engine_->stop();
    replay_bar_->hide();
    load_chart_data();
}

void MT5FleetChartPanel::on_indicator_changed(int) {
    clear_indicators();
    QString name = indicator_combo_->currentText();
    if (name == "None") return;
    if (is_oscillator(name)) add_oscillator_indicator(name);
    else add_indicator(name);
}

void MT5FleetChartPanel::on_chart_type_changed(int) {
    current_chart_type_ = chart_type_combo_->currentText();
    if (!ohlc_data_.isEmpty()) load_chart_data();
}

void MT5FleetChartPanel::on_compare_symbol_changed(int idx) {
    if (idx == 0) {
        current_compare_symbol_.clear();
        if (compare_series_) {
            chart_->removeSeries(compare_series_);
            delete compare_series_;
            compare_series_ = nullptr;
        }
    } else {
        current_compare_symbol_ = compare_combo_->currentText();
        load_compare_data(current_compare_symbol_);
    }
}

void MT5FleetChartPanel::on_log_scale_toggled(bool checked) {
    log_scale_enabled_ = checked;
    if (!ohlc_data_.isEmpty()) load_chart_data();
}

void MT5FleetChartPanel::add_indicator(const QString& name) {
    if (ohlc_data_.isEmpty()) return;

    auto* series = new QLineSeries();
    series->setName(name);

    QString key = name.toLower();
    if (key.startsWith("ema")) key = "ema" + key.mid(3);
    if (key == "bollinger") key = "bb_mid";

    // Use indicator params for period
    int period = indicator_params_.period1;
    if (name.contains("9")) period = 9;
    else if (name.contains("21")) period = 21;
    else if (name.contains("50")) period = 50;

    QColor c(255,200,100);
    if (name.contains("9")) c = QColor(100,255,150);
    else if (name.contains("21")) c = QColor(255,180,100);
    else if (name.contains("50")) c = QColor(200,150,255);
    else if (name == "RSI") c = QColor(255,100,100);
    else if (name == "MACD") c = QColor(100,200,255);

    series->setPen(QPen(c, 1.2));

    QString url = mt5_ohlc_path(current_symbol_, current_timeframe_, MAX_POINTS);
    HttpClient::instance().get(url, [this, series, name, key](Result<QJsonDocument> r) {
        if (r.is_err()) { delete series; return; }
        auto arr = r.value().object()["data"].toArray();
        if (arr.isEmpty()) { delete series; return; }

        for (auto v : arr) {
            auto obj = v.toObject();
            qreal ts = obj["time"].toDouble() * 1000;
            if (obj.contains(key) && !obj[key].isNull()) {
                qreal val = obj[key].toDouble();
                series->append(ts, val);
            }
        }

        if (series->count() > 0) {
            chart_->addSeries(series);
            if (axis_x_) series->attachAxis(axis_x_);
            if (axis_y_) series->attachAxis(axis_y_);
            indicator_series_.append(series);
        } else {
            delete series;
        }
    }, this);
}

void MT5FleetChartPanel::clear_indicators() {
    for (auto* s : indicator_series_) {
        chart_->removeSeries(s);
        delete s;
    }
    indicator_series_.clear();
    indicator_pane_->clear();
}

void MT5FleetChartPanel::add_oscillator_indicator(const QString& name) {
    if (ohlc_data_.isEmpty()) return;

    QString key = name.toLower();
    if (name == "Stochastic") key = "stoch_k";

    QString url = mt5_ohlc_path(current_symbol_, current_timeframe_, MAX_POINTS);
    HttpClient::instance().get(url, [this, name, key](Result<QJsonDocument> r) {
        if (r.is_err()) return;
        auto arr = r.value().object()["data"].toArray();
        if (arr.isEmpty()) return;

        QVector<QPointF> data;
        for (auto v : arr) {
            auto obj = v.toObject();
            qreal ts = obj["time"].toDouble() * 1000;
            if (obj.contains(key) && !obj[key].isNull()) {
                data.append(QPointF(ts, obj[key].toDouble()));
            }
        }

        if (data.isEmpty()) return;
        indicator_pane_->showIndicator(name, data);
        indicator_pane_->linkXAxis(axis_x_);

        if (!data.isEmpty()) {
            qint64 min_t = static_cast<qint64>(ohlc_data_.first().time);
            qint64 max_t = static_cast<qint64>(ohlc_data_.last().time);
            indicator_pane_->setXRange(
                QDateTime::fromMSecsSinceEpoch(min_t),
                QDateTime::fromMSecsSinceEpoch(max_t));
        }
    }, this);
}

void MT5FleetChartPanel::on_drawing_tool_clicked() {
    auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    const QString tool = btn->property("toolName").toString();
    set_active_tool(tool.isEmpty() ? btn->text() : tool);
}

void MT5FleetChartPanel::set_active_tool(const QString& toolName) {
    active_tool_name_ = toolName;
    is_elliott_mode_ = (toolName == "Elliott Wave");
    is_text_label_mode_ = (toolName == "Text");
    tool_label_->setText(toolName == "None" ? "" : toolName);
    chart_view_->setCursor(toolName == "None" ? Qt::ArrowCursor : Qt::CrossCursor);
    if (is_elliott_mode_) {
        elliott_points_.clear();
        tool_label_->setText("EW: click points, press Undo to finish");
    }
    if (is_text_label_mode_) {
        tool_label_->setText("Click chart to place text label");
    }
    for (auto* btn : drawing_tool_buttons_) {
        btn->setChecked(btn->property("toolName").toString() == toolName);
    }
    if (cursor_btn_ && (toolName == "None" || toolName.isEmpty())) cursor_btn_->setChecked(true);
}

QPointF MT5FleetChartPanel::apply_magnet_snap(const QPointF& point) const {
    if (!magnet_enabled_ || ohlc_data_.isEmpty()) return point;

    const OhlcvPoint* nearest = nullptr;
    qreal bestDistance = 1e100;
    for (const auto& candle : ohlc_data_) {
        qreal distance = qAbs(point.x() - candle.time);
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = &candle;
        }
    }
    if (!nearest) return point;

    qreal snappedY = nearest->close;
    qreal bestYDistance = qAbs(point.y() - snappedY);
    const qreal levels[] = {nearest->open, nearest->high, nearest->low, nearest->close};
    for (qreal level : levels) {
        qreal distance = qAbs(point.y() - level);
        if (distance < bestYDistance) {
            bestYDistance = distance;
            snappedY = level;
        }
    }
    return QPointF(nearest->time, snappedY);
}

void MT5FleetChartPanel::handle_object_placed(QPointF start, QPointF end) {
    DrawingObject* obj = nullptr;
    if (active_tool_name_ == "TrendLine")       obj = new TrendLineObject(start, end);
    else if (active_tool_name_ == "Ray")         obj = new RayObject(start, end);
    else if (active_tool_name_ == "H-Line")      obj = new HorizontalLineObject(start.y(), start.x(), end.x());
    else if (active_tool_name_ == "V-Line")      obj = new VerticalLineObject(start.x(), start.y(), end.y());
    else if (active_tool_name_ == "Channel")     obj = new ChannelObject(start, end);
    else if (active_tool_name_ == "Fib Retrace") obj = new FibRetraceObject(start, end);
    else if (active_tool_name_ == "Fib Ext")     obj = new FibRetraceObject(start, end);
    else if (active_tool_name_ == "Fib Fan")     obj = new GannFanObject(start);
    else if (active_tool_name_ == "Fib Arc")     obj = new FibArcObject(start, end);
    else if (active_tool_name_ == "Gann Fan") {
        qreal len = QLineF(start, end).length();
        obj = new GannFanObject(start, len > 10 ? len : 100);
    }
    else if (active_tool_name_ == "Gann Square") obj = new GannSquareObject(start, QLineF(start, end).length());
    else if (active_tool_name_ == "Pitchfork")   obj = new AndrewsPitchforkObject(start, end, QPointF((start.x()+end.x())/2, (start.y()+end.y())/2));
    else if (active_tool_name_ == "Cycle Line")  obj = new CycleLineObject(start, end);
    else if (active_tool_name_ == "Measure")     obj = new MeasureObject(start, end);

    if (obj) {
        obj->setLocked(drawings_locked_);
        obj->setVisible(!drawings_hidden_);
        chart_->scene()->addItem(obj);
        drawing_objects_.append(obj);
        connect(obj, &DrawingObject::objectSelected, this, [this](DrawingObject*) {});
    }
    last_start_ = start;
}

void MT5FleetChartPanel::handle_point_placed(QPointF pt) {
    if (is_elliott_mode_) {
        elliott_points_.append(pt);
        tool_label_->setText(QString("EW: %1 pts \u2014 Undo to finish").arg(elliott_points_.size()));
    } else if (is_text_label_mode_) {
        bool ok;
        QString text = QInputDialog::getMultiLineText(this, "Chart Annotation", "Enter text:", "", &ok);
        if (ok && !text.isEmpty()) {
            auto* label = new TextLabelObject(pt, text);
            label->setLocked(drawings_locked_);
            label->setVisible(!drawings_hidden_);
            chart_->scene()->addItem(label);
            drawing_objects_.append(label);
            connect(label, &DrawingObject::objectSelected, this, [this](DrawingObject*) {});
        }
    }
}

void MT5FleetChartPanel::handle_drawing_finished() {
    if (is_elliott_mode_ && elliott_points_.size() >= 2) {
        auto* ew = new ElliottWaveObject();
        ew->setPoints(elliott_points_);
        ew->setLocked(drawings_locked_);
        ew->setVisible(!drawings_hidden_);
        chart_->scene()->addItem(ew);
        drawing_objects_.append(ew);
        elliott_points_.clear();
    }
    set_active_tool("None");
    tool_label_->setText("");
}

void MT5FleetChartPanel::clear_drawing_tools() {
    for (auto* obj : drawing_objects_) {
        chart_->scene()->removeItem(obj);
        delete obj;
    }
    drawing_objects_.clear();
    elliott_points_.clear();
}

void MT5FleetChartPanel::undo_last_tool() {
    if (is_elliott_mode_ && !elliott_points_.isEmpty()) {
        elliott_points_.removeLast();
        tool_label_->setText(QString("EW: %1 pts \u2014 Undo to finish").arg(elliott_points_.size()));
        return;
    }
    if (drawing_objects_.isEmpty()) return;
    auto* obj = drawing_objects_.takeLast();
    chart_->scene()->removeItem(obj);
    delete obj;
    if (!is_elliott_mode_) set_active_tool("None");
}

void MT5FleetChartPanel::save_drawings() {
    QJsonArray arr;
    for (auto* obj : drawing_objects_) {
        arr.append(obj->toJson());
    }
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    QString filePath = path + "/" + current_symbol_ + "_" + current_timeframe_ + "_drawings.json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
        file.close();
        tool_label_->setText("Drawings saved");
    } else {
        tool_label_->setText("Save failed");
    }
    QTimer::singleShot(2000, this, [this](){ tool_label_->setText(""); });
}

void MT5FleetChartPanel::load_drawings() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString filePath = path + "/" + current_symbol_ + "_" + current_timeframe_ + "_drawings.json";
    QFile file(filePath);
    if (!file.exists()) { tool_label_->setText("No saved drawings"); return; }
    if (!file.open(QIODevice::ReadOnly)) { tool_label_->setText("Load failed"); return; }

    clear_drawing_tools();
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    for (auto v : doc.array()) {
        auto obj = v.toObject();
        QString tool = obj["tool"].toString();
        DrawingObject* d = nullptr;

        if (tool == "TrendLine") d = new TrendLineObject();
        else if (tool == "Ray") d = new RayObject();
        else if (tool == "H-Line") d = new HorizontalLineObject();
        else if (tool == "V-Line") d = new VerticalLineObject();
        else if (tool == "Channel") d = new ChannelObject();
        else if (tool == "Pitchfork") d = new AndrewsPitchforkObject();
        else if (tool == "Fib Retrace" || tool == "Fib Ext") d = new FibRetraceObject();
        else if (tool == "Fib Arc") d = new FibArcObject();
        else if (tool == "Gann Fan") d = new GannFanObject();
        else if (tool == "Gann Square") d = new GannSquareObject();
        else if (tool == "Cycle Line") d = new CycleLineObject();
        else if (tool == "Brush") d = new BrushObject();
        else if (tool == "Measure") d = new MeasureObject();
        else if (tool == "Elliott Wave") d = new ElliottWaveObject();
        else if (tool == "Text") d = new TextLabelObject();

        if (d) {
            d->fromJson(obj);
            d->setLocked(drawings_locked_ || d->isLocked());
            d->setVisible(!drawings_hidden_);
            chart_->scene()->addItem(d);
            drawing_objects_.append(d);
        }
    }
    tool_label_->setText(QString("Loaded %1 drawing(s)").arg(drawing_objects_.size()));
    QTimer::singleShot(2000, this, [this](){ tool_label_->setText(""); });
}

// ── New Feature Slots ──────────────────────────────────────────

void MT5FleetChartPanel::on_crosshair_toggled() {
    crosshair_enabled_ = crosshair_btn_->isChecked();
    chart_view_->setCursor(crosshair_enabled_ ? Qt::CrossCursor : Qt::ArrowCursor);
    if (!crosshair_enabled_) on_hover_leave();
}

void MT5FleetChartPanel::on_fullscreen_toggled() {
    fullscreen_enabled_ = !fullscreen_enabled_;
    if (fullscreen_enabled_) {
        window()->showFullScreen();
        fullscreen_btn_->setText("Exit FS");
    } else {
        window()->showNormal();
        fullscreen_btn_->setText("Fullscreen");
    }
}

void MT5FleetChartPanel::take_screenshot() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString fileName = QString("chart_%1_%2.png").arg(current_symbol_, QDateTime::currentMSecsSinceEpoch());
    QString fullPath = path + "/" + fileName;
    QPixmap pix = chart_view_->grab();
    if (pix.save(fullPath)) {
        tool_label_->setText("Screenshot saved");
        QApplication::clipboard()->setPixmap(pix);
    } else {
        tool_label_->setText("Screenshot failed");
    }
    QTimer::singleShot(2000, this, [this](){ tool_label_->setText(""); });
}

void MT5FleetChartPanel::on_vp_toggled() {
    vp_enabled_ = vp_btn_->isChecked();
    volume_profile_->setVisible(vp_enabled_);
    if (vp_enabled_ && !ohlc_data_.isEmpty()) {
        QVector<qreal> prices, vols;
        for (const auto& pt : ohlc_data_) { prices.append(pt.close); vols.append(pt.volume); }
        volume_profile_->setData(prices, vols);
    }
}

void MT5FleetChartPanel::on_session_toggled() {
    session_enabled_ = session_btn_->isChecked();
    if (!ohlc_data_.isEmpty()) load_chart_data();
}

void MT5FleetChartPanel::on_econ_toggled() {
    econ_enabled_ = econ_btn_->isChecked();
    if (!econ_enabled_) {
        for (auto* e : econ_event_items_) { chart_->scene()->removeItem(e); delete e; }
        econ_event_items_.clear();
    } else if (!ohlc_data_.isEmpty()) {
        render_econ_events();
    }
}

void MT5FleetChartPanel::on_bar_coloring_toggled() {
    bar_coloring_enabled_ = bar_coloring_btn_->isChecked();
    if (!ohlc_data_.isEmpty()) load_chart_data();
}

void MT5FleetChartPanel::on_chart_template_save() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    QString filePath = QFileDialog::getSaveFileName(this, "Save Chart Template",
        path + "/template.json", "JSON (*.json)");
    if (filePath.isEmpty()) return;

    QJsonObject tmpl;
    tmpl["symbol"] = current_symbol_;
    tmpl["timeframe"] = current_timeframe_;
    tmpl["chart_type"] = current_chart_type_;
    tmpl["chart_style"] = chart_style_combo_->currentText();
    tmpl["indicator"] = indicator_combo_->currentText();
    tmpl["log_scale"] = log_scale_enabled_;
    tmpl["bar_coloring"] = bar_coloring_enabled_;

    QJsonArray drawings;
    for (auto* d : drawing_objects_) drawings.append(d->toJson());
    tmpl["drawings"] = drawings;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(tmpl).toJson());
        tool_label_->setText("Template saved");
    }
    QTimer::singleShot(2000, this, [this](){ tool_label_->setText(""); });
}

void MT5FleetChartPanel::on_chart_template_load() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString filePath = QFileDialog::getOpenFileName(this, "Load Chart Template",
        path, "JSON (*.json)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonObject tmpl = QJsonDocument::fromJson(file.readAll()).object();

    if (tmpl.contains("symbol")) set_symbol(tmpl["symbol"].toString());
    if (tmpl.contains("timeframe")) set_timeframe(tmpl["timeframe"].toString());
    if (tmpl.contains("chart_type")) {
        int idx = chart_type_combo_->findText(tmpl["chart_type"].toString());
        if (idx >= 0) chart_type_combo_->setCurrentIndex(idx);
    }
    if (tmpl.contains("log_scale")) log_scale_check_->setChecked(tmpl["log_scale"].toBool());
    if (tmpl.contains("bar_coloring")) bar_coloring_btn_->setChecked(tmpl["bar_coloring"].toBool());

    if (tmpl.contains("drawings")) {
        clear_drawing_tools();
        for (auto v : tmpl["drawings"].toArray()) {
            auto dobj = v.toObject();
            QString tool = dobj["tool"].toString();
            DrawingObject* d = nullptr;
            if (tool == "TrendLine") d = new TrendLineObject();
            else if (tool == "H-Line") d = new HorizontalLineObject();
            else if (tool == "V-Line") d = new VerticalLineObject();
            else if (tool == "Channel") d = new ChannelObject();
            else if (tool == "Fib Retrace" || tool == "Fib Ext") d = new FibRetraceObject();
            else if (tool == "Gann Fan") d = new GannFanObject();
            else if (tool == "Elliott Wave") d = new ElliottWaveObject();
            else if (tool == "Text") d = new TextLabelObject();
            if (d) {
                d->fromJson(dobj);
                d->setLocked(drawings_locked_ || d->isLocked());
                d->setVisible(!drawings_hidden_);
                chart_->scene()->addItem(d);
                drawing_objects_.append(d);
            }
        }
    }

    tool_label_->setText("Template loaded");
    QTimer::singleShot(2000, this, [this](){ tool_label_->setText(""); });
}

void MT5FleetChartPanel::edit_indicator_params() {
    QString name = indicator_combo_->currentText();
    if (name == "None") return;
    IndicatorParamDialog dlg(name, indicator_params_, this);
    if (dlg.exec() == QDialog::Accepted) {
        indicator_params_ = dlg.result();
        if (!ohlc_data_.isEmpty()) on_indicator_changed(0);
    }
}

// ── Replay ───────────────────────────────────────────────────────

void MT5FleetChartPanel::on_replay_play() {
    if (replay_engine_->isPlaying()) {
        replay_engine_->pause();
        replay_play_btn_->setText("Play");
    } else {
        if (replay_engine_->totalBars() == 0) replay_engine_->loadData(chart_data_);
        replay_engine_->play();
        replay_play_btn_->setText("Pause");
        replay_bar_->show();
    }
}

void MT5FleetChartPanel::on_replay_pause() {
    replay_engine_->pause();
    replay_play_btn_->setText("Play");
}

void MT5FleetChartPanel::on_replay_stop() {
    replay_engine_->stop();
    replay_play_btn_->setText("Play");
    replay_bar_->hide();
    load_chart_data();
}

void MT5FleetChartPanel::on_replay_step_fwd() {
    if (replay_engine_->totalBars() == 0) replay_engine_->loadData(chart_data_);
    replay_engine_->stepForward();
    replay_bar_->show();
}

void MT5FleetChartPanel::on_replay_step_back() {
    replay_engine_->stepBackward();
}

void MT5FleetChartPanel::on_replay_bar_changed(int index) {
    auto visible = replay_engine_->visibleData();
    replay_label_->setText(QString("%1 / %2").arg(index + 1).arg(replay_engine_->totalBars()));

    if (candlestick_series_) {
        candlestick_series_->clear();
        for (const auto& pt : visible)
            candlestick_series_->append(new QCandlestickSet(pt.x(), pt.x(), pt.x(), pt.y(), pt.x()));
    }
    if (price_series_) {
        price_series_->clear();
        for (const auto& pt : visible) price_series_->append(pt);
    }
}

// ── Alerts ───────────────────────────────────────────────────────

void MT5FleetChartPanel::on_add_alert() {
    bool ok;
    double price = QInputDialog::getDouble(this, "Set Price Alert",
        "Alert price:", ohlc_data_.isEmpty() ? 0 : ohlc_data_.last().close, 0, 1e9, 2, &ok);
    if (!ok) return;

    QStringList conditions = {"above", "below"};
    QString condition = QInputDialog::getItem(this, "Alert Condition",
        "Trigger when price goes:", conditions, 0, false, &ok);
    if (!ok) return;

    ChartAlert alert;
    alert.timestamp = ohlc_data_.isEmpty() ? 0 : static_cast<qint64>(ohlc_data_.last().time);
    alert.price = price;
    alert.condition = condition;
    alert.label = QString("Alert $%1").arg(price, 0, 'f', 2);
    alert.color = QColor(255, 200, 100);
    alert_manager_->addAlert(alert);
    render_alerts();
    tool_label_->setText(QString("Alert set at $%1").arg(price, 0, 'f', 2));
    QTimer::singleShot(2000, this, [this](){ tool_label_->setText(""); });
}

void MT5FleetChartPanel::on_alert_triggered(const ChartAlert& alert) {
    tool_label_->setText(QString("ALERT: %1 at $%2").arg(alert.label).arg(alert.price, 0, 'f', 2));
    QTimer::singleShot(5000, this, [this](){ tool_label_->setText(""); });
}

// ── Date Range ───────────────────────────────────────────────────

void MT5FleetChartPanel::on_date_range_apply() {
    qint64 from_ms = date_from_->date().startOfDay().toMSecsSinceEpoch();
    qint64 to_ms = date_to_->date().endOfDay().toMSecsSinceEpoch();
    if (axis_x_) {
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(from_ms),
                          QDateTime::fromMSecsSinceEpoch(to_ms));
    }
}

// ── Crosshair / Hover (like CryptoChart) ─────────────────────────

void MT5FleetChartPanel::on_hover_position(const QPointF& chartVal, const QPointF& viewPos) {
    if (!chart_ || ohlc_data_.isEmpty()) return;
    if (!crosshair_enabled_) return;
    QRectF plot = chart_->plotArea();
    if (!plot.contains(viewPos)) { on_hover_leave(); return; }

    // Crosshair lines in viewport pixel space
    qreal cx = viewPos.x(), cy = viewPos.y();
    xhair_v_->setLine(cx, plot.top(), cx, plot.bottom());
    xhair_h_->setLine(plot.left(), cy, plot.right(), cy);
    xhair_v_->setVisible(true); xhair_h_->setVisible(true);
    xhair_dot_->setRect(cx - 3, cy - 3, 6, 6);
    xhair_dot_->setVisible(true);

    // Price tag (right of plot)
    double price = chartVal.y();
    price_tag_txt_->setText(QString::number(price, 'f', 2));
    QRectF tb = price_tag_txt_->boundingRect();
    qreal w = tb.width() + 12, h = tb.height() + 6;
    qreal y = qMax(plot.top(), qMin(cy - h / 2, plot.bottom() - h));
    price_tag_bg_->setRect(QRectF(plot.right() + 4, y, w, h));
    price_tag_txt_->setPos(plot.right() + 4 + 6, y + 3);
    price_tag_bg_->setVisible(true); price_tag_txt_->setVisible(true);

    // Time tag (below plot)
    qint64 ms = static_cast<qint64>(chartVal.x());
    time_tag_txt_->setText(QDateTime::fromMSecsSinceEpoch(ms).toString("dd HH:mm"));
    tb = time_tag_txt_->boundingRect();
    w = tb.width() + 12; h = tb.height() + 6;
    qreal x = qMax(plot.left(), qMin(cx - w / 2, plot.right() - w));
    time_tag_bg_->setRect(QRectF(x, plot.bottom() + 4, w, h));
    time_tag_txt_->setPos(x + 6, plot.bottom() + 4 + 3);
    time_tag_bg_->setVisible(true); time_tag_txt_->setVisible(true);

    // OHLC tooltip — find nearest candle
    qint64 best = 1e18;
    const OhlcvPoint* hit = nullptr;
    for (const auto& c : ohlc_data_) {
        qint64 d = qAbs(static_cast<qint64>(c.time) - ms);
        if (d < best) { best = d; hit = &c; }
    }
    if (hit) {
        bool up = hit->close >= hit->open;
        double pct = (hit->close - hit->open) / qMax(1e-12, hit->open) * 100;
        QString chg = QString("%1%2%").arg(pct >= 0 ? "+" : "").arg(pct, 0, 'f', 2);
        QString ts = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(hit->time)).toString("MMM dd HH:mm");
        QString dirCol = up ? "#22c55e" : "#ef4444";
        QString dim = "#808080", fg = "#e5e5e5";

        ohlc_tooltip_->setText(QString(
            "<div style='font-family:Consolas;'>"
            "<div style='color:%1;font-size:10px;font-weight:700;margin-bottom:4px;'>%2</div>"
            "<table cellspacing='0' cellpadding='0' style='font-size:12px;'>"
            "<tr><td style='color:%1;padding-right:10px;'>O</td><td style='color:%3;text-align:right;'>%4</td></tr>"
            "<tr><td style='color:%1;padding-right:10px;'>H</td><td style='color:%5;text-align:right;'>%6</td></tr>"
            "<tr><td style='color:%1;padding-right:10px;'>L</td><td style='color:%7;text-align:right;'>%8</td></tr>"
            "<tr><td style='color:%1;padding-right:10px;'>C</td><td style='color:%9;text-align:right;font-weight:700;'>%10</td></tr>"
            "<tr><td colspan='2' style='padding-top:4px;color:%9;font-weight:700;text-align:right;border-top:1px solid #333;'>%11</td></tr>"
            "</table></div>")
            .arg(dim).arg(ts).arg(fg)
            .arg(hit->open, 0, 'f', 2)
            .arg("#22c55e").arg(hit->high, 0, 'f', 2)
            .arg("#ef4444").arg(hit->low, 0, 'f', 2)
            .arg(dirCol).arg(hit->close, 0, 'f', 2)
            .arg(chg));
        ohlc_tooltip_->adjustSize();
        ohlc_tooltip_->setVisible(true);
    }
}

void MT5FleetChartPanel::on_hover_leave() {
    if (xhair_v_) xhair_v_->setVisible(false);
    if (xhair_h_) xhair_h_->setVisible(false);
    if (xhair_dot_) xhair_dot_->setVisible(false);
    if (price_tag_bg_) price_tag_bg_->setVisible(false);
    if (price_tag_txt_) price_tag_txt_->setVisible(false);
    if (time_tag_bg_) time_tag_bg_->setVisible(false);
    if (time_tag_txt_) time_tag_txt_->setVisible(false);
    if (ohlc_tooltip_) ohlc_tooltip_->setVisible(false);
    update_last_price_marker();
}

void MT5FleetChartPanel::update_last_price_marker() {
    if (!chart_ || ohlc_data_.isEmpty() || !last_tag_bg_) {
        if (last_tag_bg_) last_tag_bg_->setVisible(false);
        return;
    }
    double last_close = ohlc_data_.last().close;
    QRectF plot = chart_->plotArea();
    last_tag_txt_->setText(QString::number(last_close, 'f', 2));
    QRectF tb = last_tag_txt_->boundingRect();
    qreal w = tb.width() + 12, h = tb.height() + 6;
    qreal y = qMax(plot.top(), qMin(chart_->mapToPosition(QPointF(ohlc_data_.last().time, last_close)).y() - h / 2, plot.bottom() - h));
    last_tag_bg_->setRect(QRectF(plot.right() + 4, y, w, h));
    last_tag_txt_->setPos(plot.right() + 4 + 6, y + 3);
    last_tag_bg_->setVisible(true);
    last_tag_txt_->setVisible(true);
}

// ── Live Positions ──────────────────────────────────────────────

void MT5FleetChartPanel::refresh_positions() {
    if (current_symbol_.isEmpty()) return;
    HttpClient::instance().get("/mt5/positions",
        [this](Result<QJsonDocument> r) {
            if (!r.is_ok()) return;
            auto arr = r.value().object()["data"].toArray();
            if (arr.isEmpty()) return;

            // Clear old position markers
            for (auto* t : order_text_items_) { chart_->scene()->removeItem(t); delete t; }
            order_text_items_.clear();
            qDeleteAll(order_marker_series_); order_marker_series_.clear();

            double total_pnl = 0;
            for (auto v : arr) {
                auto pos = v.toObject();
                if (pos["symbol"].toString() != current_symbol_) continue;

                QString side = pos["side"].toString();
                double volume = pos["volume"].toDouble();
                double entry = pos["entry"].toDouble();
                double current = pos["current"].toDouble();
                double pnl = pos["profit"].toDouble();
                double sl = pos["sl"].toDouble();
                double tp = pos["tp"].toDouble();
                qint64 time = static_cast<qint64>(pos["time"].toDouble()) * 1000;
                total_pnl += pnl;

                QColor c = (side == "BUY") ? QColor(ui::colors::POSITIVE()) : QColor(ui::colors::NEGATIVE());

                // Marker line at entry
                auto* marker = new QLineSeries();
                marker->setPen(QPen(c, 2, Qt::SolidLine));
                qreal y_bottom = axis_y_ ? axisMin(axis_y_) : 0;
                qreal y_top = axis_y_ ? axisMax(axis_y_) : 1;
                marker->append(static_cast<qreal>(time), y_bottom);
                marker->append(static_cast<qreal>(time), entry);
                chart_->addSeries(marker);
                marker->attachAxis(axis_x_);
                marker->attachAxis(axis_y_);
                order_marker_series_.append(marker);

                // Entry price line
                auto* entry_line = new QLineSeries();
                entry_line->setPen(QPen(c, 1, Qt::DashLine));
                entry_line->append(static_cast<qreal>(time), entry);
                entry_line->append(static_cast<qreal>(QDateTime::currentMSecsSinceEpoch()), entry);
                chart_->addSeries(entry_line);
                entry_line->attachAxis(axis_x_);
                entry_line->attachAxis(axis_y_);
                order_marker_series_.append(entry_line);

                // Label
                auto* text = new QGraphicsSimpleTextItem(
                    QString("%1 %2x\nP&L: $%3")
                        .arg(side).arg(volume, 0, 'f', 2).arg(pnl, 0, 'f', 0));
                text->setBrush(QBrush(c));
                text->setFont(QFont("Consolas", 8));
                QPointF labelPos = chart_->mapToPosition(QPointF(static_cast<qreal>(time), entry));
                text->setPos(labelPos + QPointF(5, -20));
                chart_->scene()->addItem(text);
                order_text_items_.append(text);

                // SL/TP labels
                if (sl > 0) {
                    auto* sl_text = new QGraphicsSimpleTextItem(QString("SL $%1").arg(sl, 0, 'f', 2));
                    sl_text->setBrush(QBrush(QColor(255, 80, 80)));
                    sl_text->setFont(QFont("Consolas", 7));
                    QPointF slPos = chart_->mapToPosition(QPointF(static_cast<qreal>(time), sl));
                    sl_text->setPos(slPos + QPointF(5, 0));
                    chart_->scene()->addItem(sl_text);
                    order_text_items_.append(sl_text);
                }
                if (tp > 0) {
                    auto* tp_text = new QGraphicsSimpleTextItem(QString("TP $%1").arg(tp, 0, 'f', 2));
                    tp_text->setBrush(QBrush(QColor(80, 255, 80)));
                    tp_text->setFont(QFont("Consolas", 7));
                    QPointF tpPos = chart_->mapToPosition(QPointF(static_cast<qreal>(time), tp));
                    tp_text->setPos(tpPos + QPointF(5, 0));
                    chart_->scene()->addItem(tp_text);
                    order_text_items_.append(tp_text);
                }
            }

            // Update price label with P&L
            price_label_->setText(QString("$%1 | P&L: $%2")
                .arg(ohlc_data_.isEmpty() ? 0 : ohlc_data_.last().close, 0, 'f', 2)
                .arg(total_pnl, 0, 'f', 0));
        }, this);
}

// ── Event Filter (drawing tools on CryptoChart) ─────────────────

bool MT5FleetChartPanel::eventFilter(QObject* obj, QEvent* event) {
    if (obj == chart_view_ && active_tool_name_ != "None") {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                QPointF chartPt = chart_->mapToValue(me->position());
                chartPt = apply_magnet_snap(chartPt);
                if (active_tool_name_ == "Elliott Wave" || active_tool_name_ == "Text") {
                    handle_point_placed(chartPt);
                } else {
                    is_placing_ = true;
                    last_start_ = chartPt;
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease && is_placing_) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                is_placing_ = false;
                QPointF endPt = chart_->mapToValue(me->position());
                endPt = apply_magnet_snap(endPt);
                handle_object_placed(last_start_, endPt);
                handle_drawing_finished();
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Context Menu ─────────────────────────────────────────────────

void MT5FleetChartPanel::show_chart_context_menu(const QPoint& pos) {
    if (context_menu_) context_menu_->popup(chart_view_->mapToGlobal(pos));
}

// ── Paper Trade ─────────────────────────────────────────────────

void MT5FleetChartPanel::on_paper_trade() {
    if (ohlc_data_.size() < 30) { tool_label_->setText("Need more data"); return; }

    QString strategy = paper_strategy_combo_ ? paper_strategy_combo_->currentText() : "EMA Crossover";
    int fast = 9, slow = 21;
    if (strategy == "SMA Crossover") { fast = 50; slow = 200; }

    // Compute MAs
    QVector<double> ma_fast(ohlc_data_.size(), 0), ma_slow(ohlc_data_.size(), 0);
    if (strategy == "SMA Crossover") {
        // Simple moving average
        for (int i = 0; i < ohlc_data_.size(); ++i) {
            double s_f = 0, s_s = 0;
            int c_f = 0, c_s = 0;
            for (int j = qMax(0, i - fast + 1); j <= i; ++j) { s_f += ohlc_data_[j].close; c_f++; }
            for (int j = qMax(0, i - slow + 1); j <= i; ++j) { s_s += ohlc_data_[j].close; c_s++; }
            ma_fast[i] = c_f > 0 ? s_f / c_f : ohlc_data_[i].close;
            ma_slow[i] = c_s > 0 ? s_s / c_s : ohlc_data_[i].close;
        }
    } else {
        // Exponential moving average
        double k_f = 2.0 / (fast + 1), k_s = 2.0 / (slow + 1);
        ma_fast[0] = ohlc_data_[0].close;
        ma_slow[0] = ohlc_data_[0].close;
        for (int i = 1; i < ohlc_data_.size(); ++i) {
            double c = ohlc_data_[i].close;
            ma_fast[i] = c * k_f + ma_fast[i-1] * (1 - k_f);
            ma_slow[i] = c * k_s + ma_slow[i-1] * (1 - k_s);
        }
    }

    // ATR for SL/TP
    QVector<double> atr(ohlc_data_.size(), 0);
    double sum_tr = 0;
    for (int i = 1; i <= 14 && i < ohlc_data_.size(); ++i) {
        const auto& p = ohlc_data_[i];
        const auto& q = ohlc_data_[i-1];
        double tr = qMax(p.high - p.low, qMax(qAbs(p.high - q.close), qAbs(p.low - q.close)));
        sum_tr += tr;
        atr[i] = sum_tr / i;
    }
    for (int i = 15; i < ohlc_data_.size(); ++i) {
        const auto& p = ohlc_data_[i];
        const auto& q = ohlc_data_[i-1];
        double tr = qMax(p.high - p.low, qMax(qAbs(p.high - q.close), qAbs(p.low - q.close)));
        atr[i] = (atr[i-1] * 13 + tr) / 14;
    }

    // Generate signals
    QVector<OrderMarker> markers;
    double balance = 100000, peak = balance, dd = 0;
    int wins = 0, losses = 0;
    bool in_pos = false;
    double entry_price = 0, entry_time = 0;
    QString entry_side;
    double gross_pnl = 0;

    for (int i = slow + 1; i < ohlc_data_.size(); ++i) {
        double f0 = ma_fast[i], f1 = ma_fast[i-1];
        double s0 = ma_slow[i], s1 = ma_slow[i-1];
        double a = atr[i];
        bool buy = (f1 <= s1 && f0 > s0);
        bool sell = (f1 >= s1 && f0 < s0);

        if (buy && !in_pos) {
            entry_price = ohlc_data_[i].close;
            entry_time = ohlc_data_[i].time;
            entry_side = "BUY";
            in_pos = true;
            markers.append({static_cast<qint64>(entry_time), entry_price, "BUY", "Entry", 1.0});
        } else if (sell && !in_pos) {
            entry_price = ohlc_data_[i].close;
            entry_time = ohlc_data_[i].time;
            entry_side = "SELL";
            in_pos = true;
            markers.append({static_cast<qint64>(entry_time), entry_price, "SELL", "Entry", 1.0});
        } else if (in_pos) {
            double exit_price = 0;
            QString exit_reason;
            double atr_sl = a * 1.5;
            double atr_tp = a * 3.0;

            if (entry_side == "BUY") {
                double sl = entry_price - atr_sl;
                double tp = entry_price + atr_tp;
                if (sell || ohlc_data_[i].low <= sl) {
                    exit_price = sell ? ohlc_data_[i].close : sl;
                    exit_reason = sell ? "Signal" : "SL";
                } else if (ohlc_data_[i].high >= tp) {
                    exit_price = tp;
                    exit_reason = "TP";
                }
            } else {
                double sl = entry_price + atr_sl;
                double tp = entry_price - atr_tp;
                if (buy || ohlc_data_[i].high >= sl) {
                    exit_price = buy ? ohlc_data_[i].close : sl;
                    exit_reason = buy ? "Signal" : "SL";
                } else if (ohlc_data_[i].low <= tp) {
                    exit_price = tp;
                    exit_reason = "TP";
                }
            }

            if (exit_price > 0) {
                double pnl = (entry_side == "BUY") ?
                    (exit_price - entry_price) : (entry_price - exit_price);
                pnl *= 100; // 1 lot XAUUSD = $100 per point
                gross_pnl += pnl;
                balance += pnl;
                if (pnl > 0) wins++; else losses++;
                peak = qMax(peak, balance);
                dd = qMax(dd, peak - balance);

                OrderMarker m;
                m.time = static_cast<qint64>(ohlc_data_[i].time);
                m.price = exit_price;
                m.side = (entry_side == "BUY") ? "SELL" : "BUY";
                m.label = exit_reason;
                m.lot = 1.0;
                markers.append(m);

                in_pos = false;
            }
        }
    }

    // Apply markers to chart
    order_markers_ = markers;
    if (!ohlc_data_.isEmpty()) {
        qDeleteAll(order_marker_series_); order_marker_series_.clear();
        for (auto* t : order_text_items_) { chart_->scene()->removeItem(t); delete t; }
        order_text_items_.clear();
        render_order_markers();
    }

    // Show results
    int total = wins + losses;
    double winrate = total > 0 ? (double)wins / total * 100 : 0;
    tool_label_->setText(QString("PaperTrade: %1 trades | Win %2% | P&L $%3 | DD $%4 | Bal $%5")
        .arg(total).arg(winrate, 0, 'f', 1).arg(gross_pnl, 0, 'f', 0)
        .arg(dd, 0, 'f', 0).arg(balance, 0, 'f', 0));
}

} // namespace fincept::screens
