#pragma once
#include <QWidget>
#include <QWebSocket>
#include <QLineSeries>
#include <QChartView>
#include <QChart>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace fincept::screens {

class TickReplayWidget : public QWidget {
    Q_OBJECT
  public:
    explicit TickReplayWidget(QWidget* parent = nullptr);
    ~TickReplayWidget() override;

  private slots:
    void on_disconnected();
    void on_text_message(const QString& message);
    void on_start_replay();

  private:
    void build_ui();

    QWebSocket* ws_ = nullptr;
    QChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QLineSeries* equity_series_ = nullptr;
    QValueAxis* axis_y_ = nullptr;
    QDateTimeAxis* axis_x_ = nullptr;

    QLineEdit* data_dir_edit_ = nullptr;
    QLineEdit* symbols_edit_ = nullptr;
    QDoubleSpinBox* cash_spin_ = nullptr;
    QPushButton* connect_btn_ = nullptr;
    QPushButton* start_btn_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* trade_count_label_ = nullptr;
    QLabel* equity_label_ = nullptr;
    bool connected_ = false;
    bool running_ = false;
    qreal peak_equity_ = 0;
};

} // namespace fincept::screens
