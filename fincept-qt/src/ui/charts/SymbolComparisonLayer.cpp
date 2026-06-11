#include "ui/charts/SymbolComparisonLayer.h"

#include <QChart>
#include <QPen>

namespace fincept::ui {

SymbolComparison::SymbolComparison(QChart* chart, QObject* parent)
    : QObject(parent), chart_(chart) {}

void SymbolComparison::add_symbol(const QString& symbol, const QColor& color) {
    if (has_symbol(symbol)) return;
    CompareSymbol cs;
    cs.symbol = symbol;
    cs.color = color;
    cs.series.append(new QLineSeries());
    cs.series.last()->setPen(QPen(color, 1.5));
    cs.series.last()->setName(symbol);
    if (chart_) chart_->addSeries(cs.series.last());
    symbols_.append(cs);
    emit symbol_added(symbol);
}

void SymbolComparison::remove_symbol(const QString& symbol) {
    for (int i = 0; i < symbols_.size(); ++i) {
        if (symbols_[i].symbol == symbol) {
            for (auto* s : symbols_[i].series) {
                if (chart_) chart_->removeSeries(s);
                delete s;
            }
            symbols_.removeAt(i);
            emit symbol_removed(symbol);
            return;
        }
    }
}

void SymbolComparison::clear() {
    for (auto& cs : symbols_) {
        for (auto* s : cs.series) {
            if (chart_) chart_->removeSeries(s);
            delete s;
        }
    }
    symbols_.clear();
}

bool SymbolComparison::has_symbol(const QString& symbol) const {
    for (const auto& cs : symbols_)
        if (cs.symbol == symbol) return true;
    return false;
}

void SymbolComparison::set_data(const QString& symbol, const QVector<double>& timestamps_ms,
                                 const QVector<double>& prices) {
    for (auto& cs : symbols_) {
        if (cs.symbol == symbol) {
            for (auto* s : cs.series) {
                s->clear();
                QVector<QPointF> pts;
                double ref_price = prices.isEmpty() ? 1 : prices.first();
                for (int i = 0; i < timestamps_ms.size() && i < prices.size(); ++i) {
                    // Normalize to percentage change from first price
                    double pct = ref_price > 0 ? ((prices[i] / ref_price) - 1.0) * 100 : 0;
                    pts.append(QPointF(timestamps_ms[i], pct));
                }
                s->replace(pts);
            }
            break;
        }
    }
}

} // namespace fincept::ui
