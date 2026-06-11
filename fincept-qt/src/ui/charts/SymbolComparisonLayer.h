#pragma once

#include <QColor>
#include <QLineSeries>
#include <QObject>
#include <QString>
#include <QVector>

class QChart;
class QValueAxis;

namespace fincept::ui {

struct CompareSymbol {
    QString symbol;
    QColor color;
    QVector<QLineSeries*> series;
    double scale_factor = 1.0;
};

class SymbolComparison : public QObject {
    Q_OBJECT
public:
    explicit SymbolComparison(QChart* chart, QObject* parent = nullptr);

    void add_symbol(const QString& symbol, const QColor& color);
    void remove_symbol(const QString& symbol);
    void clear();

    void set_data(const QString& symbol, const QVector<double>& timestamps_ms,
                  const QVector<double>& prices);
    bool has_symbol(const QString& symbol) const;

signals:
    void symbol_added(const QString& symbol);
    void symbol_removed(const QString& symbol);

private:
    QChart* chart_;
    QVector<CompareSymbol> symbols_;
};

} // namespace fincept::ui
