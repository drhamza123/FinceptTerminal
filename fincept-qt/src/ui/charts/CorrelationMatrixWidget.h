#pragma once

#include <QTableWidget>
#include <QVector>
#include <QPair>

namespace fincept::ui {

class CorrelationMatrixWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit CorrelationMatrixWidget(QWidget* parent = nullptr);
    void set_symbols(const QStringList& symbols);
    void set_correlation(int row, int col, double value);
    void compute_from_returns(const QVector<QPair<QString, QVector<double>>>& symbol_returns);

private:
    QStringList symbols_;
    void color_cell(int row, int col, double value);
};

} // namespace fincept::ui
