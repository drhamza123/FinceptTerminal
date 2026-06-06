// src/screens/algo_trading/MT5FleetBlotter.h
#pragma once
#include "screens/algo_trading/MT5FleetTypes.h"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class MT5FleetBlotter : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetBlotter(QWidget* parent = nullptr);
    void set_instances(const QVector<mt5::EAInstance>& instances);
    void set_filter(const QString& text);
    void refresh_theme();

  signals:
    void instance_selected(QString eid, QString symbol);
    void chart_requested(QString eid, QString symbol);

  private:
    void build_ui();
    void retranslateUi();
    void apply_filters();

    QLabel* title_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLineEdit* filter_edit_ = nullptr;
    QTableWidget* table_ = nullptr;
    QVector<mt5::EAInstance> instances_;
    QString filter_text_;
};

} // namespace fincept::screens
