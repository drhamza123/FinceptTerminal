#pragma once
#include <QWidget>
#include <QWebSocket>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace fincept::screens {

class MT5FleetChartPanel;

class PolygonOverlay : public QWidget {
    Q_OBJECT
  public:
    explicit PolygonOverlay(MT5FleetChartPanel* chart, QWidget* parent = nullptr);
    ~PolygonOverlay() override;

  private slots:
    void on_connect();
    void on_ws_message(const QString& msg);

  private:
    void build_ui();
    MT5FleetChartPanel* chart_ = nullptr;
    QWebSocket* ws_ = nullptr;
    QLineEdit* sym_edit_ = nullptr;
    QPushButton* conn_btn_ = nullptr;
    QLabel* last_lbl_ = nullptr;
    QLabel* bid_lbl_ = nullptr;
    QLabel* ask_lbl_ = nullptr;
    QLabel* vol_lbl_ = nullptr;
    QLabel* ts_lbl_ = nullptr;
    QLabel* status_lbl_ = nullptr;
    bool connected_ = false;
};

} // namespace fincept::screens
