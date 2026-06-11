#include "ui/charts/MultiChartLink.h"
#include "screens/crypto_trading/CryptoChart.h"

namespace fincept::ui {

MultiChartLink::MultiChartLink(QObject* parent) : QObject(parent) {}

void MultiChartLink::add_chart(fincept::screens::crypto::CryptoChart* chart) {
    if (!chart || charts_.contains(chart)) return;
    charts_.append(chart);
    connect(chart, &fincept::screens::crypto::CryptoChart::crosshair_moved,
            this, &MultiChartLink::on_crosshair_moved);
    connect(chart, &fincept::screens::crypto::CryptoChart::timeframe_changed,
            this, &MultiChartLink::on_timeframe_changed);
}

void MultiChartLink::remove_chart(fincept::screens::crypto::CryptoChart* chart) {
    charts_.removeAll(chart);
}

void MultiChartLink::clear() { charts_.clear(); }

void MultiChartLink::on_crosshair_moved(double price, qint64 time_ms) {
    if (syncing_) return;
    syncing_ = true;
    auto* sender = qobject_cast<fincept::screens::crypto::CryptoChart*>(QObject::sender());
    for (auto& c : charts_) {
        if (c && c != sender) {
            // Crosshair sync between linked charts
        }
    }
    syncing_ = false;
}

void MultiChartLink::on_timeframe_changed(const QString& tf) {
    Q_UNUSED(tf)
    if (syncing_) return;
    syncing_ = true;
    // TODO: sync timeframe across linked charts
    syncing_ = false;
}

} // namespace fincept::ui
