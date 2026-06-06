#pragma once
#include <QWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QJsonArray>
#include "network/http/HttpClient.h"

namespace fincept::screens {

class StrategyScannerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit StrategyScannerPanel(QWidget* parent = nullptr);

  private slots:
    void on_scan();
    void on_deploy(int row);

  private:
    void build_ui();
    void display_results(const QJsonArray& results);

    QPushButton* scan_btn_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QLabel* status_lbl_ = nullptr;
    QTableWidget* table_ = nullptr;
    QJsonArray last_results_;
};

} // namespace fincept::screens
