// src/screens/algo_trading/MT5FleetPanel.h
#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QWebSocket>
#include <QAbstractSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QTimer>
#include <QLineEdit>
#include <QComboBox>
#include <QtCharts/QChartView>
#include <QtCharts/QCandlestickSeries>
#include <QtCharts/QCandlestickSet>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QSplitter>

#include "network/http/HttpClient.h"

namespace fincept::screens {

inline constexpr auto CYAN_HEX = "#00E5FF";
inline constexpr auto AMBER_HEX = "#FFC400";

class MT5FleetPanel : public QWidget {
    Q_OBJECT
public:
    explicit MT5FleetPanel(QWidget* parent = nullptr);

public slots:
    void refreshFleet();

private slots:
    void onFleetRowClicked(int row, int col);
    void onTabChanged(int idx);

private:
    void setupUi();
    void connectWebSocket();
    void refreshEAs();
    void updatePriceView(const QString& symbol);
    void updateChart(const QString& symbol);
    void clearChart();
    void updateStats();

    // ── UI Sections (Portfolio layout) ──

    // Command bar
    QWidget* cmd_bar_;
    QPushButton* btn_refresh_;
    QPushButton* btn_kill_all_;
    QLabel* label_ws_;
    QLabel* label_title_;
    QComboBox* view_selector_;

    // Stats ribbon
    QWidget* stats_bar_;
    QLabel* stat_connected_;
    QLabel* stat_pnl_;
    QLabel* stat_win_rate_;
    QLabel* stat_total_trades_;
    QLabel* stat_balance_;
    QLabel* stat_today_pnl_;

    // Content stack
    QStackedWidget* content_stack_;
    QWidget* ea_view_;
    QTabWidget* market_tabs_;
    QTimer* market_timer_;
    QVector<QTableWidget*> market_tables_;

    // EA table
    QTableWidget* ea_table_;
    QLabel* count_label_;

    // Chart / detail panel
    QWidget* chart_panel_;
    QChartView* chart_view_;
    QChart* chart_;
    QLabel* chart_title_;
    QLabel* chart_info_;
    QPushButton* chart_close_;
    QString selected_symbol_;

    // Status bar
    QWidget* status_bar_;
    QLabel* status_engine_;
    QLabel* status_time_;

    // Network
    QWebSocket* ws_;
    QTimer* reconnect_timer_;
    QString api_base_;
    bool ws_connected_;
    QTimer* refresh_timer_;
};

} // namespace fincept::screens
