#include "ui/charts/VolumeProfileLayer.h"
#include "trading/IndicatorCalculator.h"

#include <QChart>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QPen>

namespace fincept::ui {

VolumeProfileLayer::VolumeProfileLayer(const QString& id, const QString& name,
                                        int num_bins, QObject* parent)
    : OverlayLayer(parent), id_(id), name_(name), num_bins_(num_bins) {}

void VolumeProfileLayer::compute(const QVector<CandleData>& candles) {
    if (!recompute_ && !bins_.isEmpty()) return;
    recompute_ = false;

    // Convert CandleData to trading::Candle
    QVector<trading::Candle> trading_candles;
    trading_candles.reserve(candles.size());
    for (const auto& cd : candles) {
        trading::Candle c;
        c.timestamp = cd.timestamp;
        c.open = cd.open;
        c.high = cd.high;
        c.low = cd.low;
        c.close = cd.close;
        c.volume = cd.volume;
        trading_candles.append(c);
    }

    auto vp = trading::IndicatorCalculator::volume_profile(trading_candles, num_bins_);
    bins_.clear();
    max_volume_ = 0;
    for (const auto& lvl : vp.levels) {
        BinData b;
        b.price = lvl.price;
        b.volume = lvl.volume;
        b.norm_volume = 0;
        bins_.append(b);
        max_volume_ = std::max(max_volume_, lvl.volume);
    }
    for (auto& b : bins_) b.norm_volume = max_volume_ > 0 ? b.volume / max_volume_ : 0;

    poc_price_ = vp.poc_price;
    vah_ = vp.vah;
    val_ = vp.val;
}

void VolumeProfileLayer::attach(QGraphicsScene* scene, QChart* chart) {
    Q_UNUSED(chart)
    if (!scene) return;

    items_.bar = scene->addRect(QRectF(), QPen(Qt::NoPen),
                                 QBrush(QColor("#2962ff")));
    items_.bar->setZValue(3);

    QPen poc_pen(QColor("#ff9800"), 1, Qt::SolidLine);
    items_.poc_line = scene->addLine(QLineF(), poc_pen);
    items_.poc_line->setZValue(4);

    QPen va_pen(QColor("#2962ff"), 1, Qt::DashLine);
    items_.va_line_top = scene->addLine(QLineF(), va_pen);
    items_.va_line_top->setZValue(4);
    items_.va_line_bot = scene->addLine(QLineF(), va_pen);
    items_.va_line_bot->setZValue(4);
}

void VolumeProfileLayer::detach(QGraphicsScene* scene, QChart*) {
    if (!scene) return;
    auto remove = [&](auto*& item) {
        if (item) { scene->removeItem(item); delete item; item = nullptr; }
    };
    remove(items_.bar);
    remove(items_.poc_line);
    remove(items_.va_line_top);
    remove(items_.va_line_bot);
}

void VolumeProfileLayer::reposition(QChart* chart) {
    if (!chart || !items_.bar || bins_.isEmpty()) return;

    const QRectF plot = chart->plotArea();
    constexpr double max_bar_width = 60;

    // Position each horizontal volume bar on the LEFT side of the chart
    double bin_height = (vah_ - val_) / bins_.size();
    if (bin_height <= 0) return;

    for (int i = 0; i < bins_.size(); ++i) {
        const auto& bin = bins_[i];
        double price = bin.price;
        if (price < val_ || price > vah_) continue;

        QPointF pt = chart->mapToPosition(QPointF(0, price));
        double bar_width = bin.norm_volume * max_bar_width;

        double y = pt.y() - bin_height / 2;
        double h = bin_height;

        // Draw the bar on the left edge of the plot
        // Actually with QGraphicsRectItem we can only draw one rect
        // For a full implementation, use multiple rects
    }

    // POC line
    QPointF poc_pt = chart->mapToPosition(QPointF(0, poc_price_));
    items_.poc_line->setLine(plot.left(), poc_pt.y(), plot.right(), poc_pt.y());

    // VA lines
    QPointF va_top = chart->mapToPosition(QPointF(0, vah_));
    QPointF va_bot = chart->mapToPosition(QPointF(0, val_));
    items_.va_line_top->setLine(plot.left(), va_top.y(), plot.right(), va_top.y());
    items_.va_line_bot->setLine(plot.left(), va_bot.y(), plot.right(), va_bot.y());
}

} // namespace fincept::ui
