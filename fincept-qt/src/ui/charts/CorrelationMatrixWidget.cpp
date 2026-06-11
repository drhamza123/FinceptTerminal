#include "ui/charts/CorrelationMatrixWidget.h"

#include <QHeaderView>
#include <QtMath>
#include <cmath>

namespace fincept::ui {

CorrelationMatrixWidget::CorrelationMatrixWidget(QWidget* parent) : QTableWidget(parent) {
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    verticalHeader()->hide();
    horizontalHeader()->setStretchLastSection(true);
    setMinimumHeight(200);
}

void CorrelationMatrixWidget::set_symbols(const QStringList& symbols) {
    symbols_ = symbols;
    int n = symbols.size();
    setRowCount(n);
    setColumnCount(n);
    setHorizontalHeaderLabels(symbols);
    setVerticalHeaderLabels(symbols);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            color_cell(r, c, r == c ? 1.0 : 0.0);
}

void CorrelationMatrixWidget::set_correlation(int row, int col, double value) {
    color_cell(row, col, value);
    color_cell(col, row, value);
}

void CorrelationMatrixWidget::compute_from_returns(
    const QVector<QPair<QString, QVector<double>>>& symbol_returns)
{
    int n = symbol_returns.size();
    set_symbols([&]() { QStringList l; for (auto& s : symbol_returns) l.append(s.first); return l; }());
    for (int i = 0; i < n; ++i) {
        for (int j = i; j < n; ++j) {
            const auto& a = symbol_returns[i].second;
            const auto& b = symbol_returns[j].second;
            int m = qMin(a.size(), b.size());
            if (m < 2) continue;
            double mean_a = 0, mean_b = 0;
            for (int k = 0; k < m; ++k) { mean_a += a[k]; mean_b += b[k]; }
            mean_a /= m; mean_b /= m;
            double cov = 0, var_a = 0, var_b = 0;
            for (int k = 0; k < m; ++k) {
                double da = a[k] - mean_a, db = b[k] - mean_b;
                cov += da * db; var_a += da * da; var_b += db * db;
            }
            double denom = std::sqrt(var_a * var_b);
            double corr = denom == 0 ? 0 : cov / denom;
            set_correlation(i, j, corr);
        }
    }
}

void CorrelationMatrixWidget::color_cell(int row, int col, double value) {
    auto* item = new QTableWidgetItem(QString::number(value, 'f', 2));
    item->setTextAlignment(Qt::AlignCenter);
    int g = static_cast<int>(255 - std::abs(value) * 200);
    if (value > 0) item->setBackground(QBrush(QColor(0, g, 100)));
    else if (value < 0) item->setBackground(QBrush(QColor(200, g, 0)));
    else item->setBackground(QBrush(QColor(40, 40, 40)));
    item->setForeground(QBrush(Qt::white));
    setItem(row, col, item);
}

} // namespace fincept::ui
