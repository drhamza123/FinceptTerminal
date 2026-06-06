#pragma once
#include <QWidget>
#include <QWebSocket>
#include <QTimer>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QChartView>
#include <QJsonDocument>
#include <QMutex>
#include <QVector>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace fincept::screens {

struct ReplayPoint {
    qint64 ts = 0;
    double equity = 0;
    double z_score = 0;
};

class BacktestVisualizer : public QWidget {
    Q_OBJECT
  public:
    explicit BacktestVisualizer(QWidget* parent = nullptr);
    ~BacktestVisualizer() override;

  public slots:
    void startBacktest(const QString& symbols, const QString& dataDir, double cash);
    void stopBacktest();

  private slots:
    void onConnected();
    void onTextMessage(const QString& msg);
    void onFlush();

  private:
    void build_ui();

    QWebSocket* ws_ = nullptr;
    QTimer* flush_timer_ = nullptr;

    QChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QLineSeries* equity_series_ = nullptr;
    QLineSeries* zscore_series_ = nullptr;
    QValueAxis* axis_x_ = nullptr;
    QValueAxis* axis_y_eq_ = nullptr;
    QValueAxis* axis_y_z_ = nullptr;

    QPushButton* start_btn_ = nullptr;
    QPushButton* stop_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* equity_label_ = nullptr;
    QLabel* zscore_label_ = nullptr;

    QMutex mutex_;
    QVector<ReplayPoint> buffer_;
    ReplayPoint latest_;
    bool has_data_ = false;
    bool running_ = false;
    double peak_eq_ = 0;
};

} // namespace fincept::screens
