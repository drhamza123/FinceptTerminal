#pragma once

#include <QObject>
#include <QVector>
#include <QPointer>

namespace fincept::screens::crypto { class CryptoChart; }

namespace fincept::ui {

class MultiChartLink : public QObject {
    Q_OBJECT
public:
    explicit MultiChartLink(QObject* parent = nullptr);

    void add_chart(fincept::screens::crypto::CryptoChart* chart);
    void remove_chart(fincept::screens::crypto::CryptoChart* chart);
    void clear();
    int count() const { return charts_.size(); }

private:
    QVector<QPointer<fincept::screens::crypto::CryptoChart>> charts_;
    bool syncing_ = false;

    void on_crosshair_moved(double price, qint64 time_ms);
    void on_timeframe_changed(const QString& tf);
};

} // namespace fincept::ui
