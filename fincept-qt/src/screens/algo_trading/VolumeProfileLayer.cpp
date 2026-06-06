#include "screens/algo_trading/VolumeProfileLayer.h"

#include <QPainter>
#include <QPaintEvent>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::screens {

VolumeProfileLayer::VolumeProfileLayer(QWidget* parent)
    : QWidget(parent)
{
    setMinimumWidth(80);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void VolumeProfileLayer::setData(const QVector<qreal>& prices, const QVector<qreal>& volumes, int numBins)
{
    if (prices.isEmpty() || volumes.isEmpty() || prices.size() != volumes.size()) {
        return;
    }

    qreal minP = *std::min_element(prices.begin(), prices.end());
    qreal maxP = *std::max_element(prices.begin(), prices.end());
    if (qFuzzyCompare(minP, maxP)) {
        return;
    }

    qreal binWidth = (maxP - minP) / numBins;
    QVector<qreal> binVolumes(numBins, 0);
    QVector<int> binCounts(numBins, 0);

    for (int i = 0; i < prices.size(); ++i) {
        int bin = std::min(static_cast<int>((prices[i] - minP) / binWidth), numBins - 1);
        binVolumes[bin] += volumes[i];
        binCounts[bin]++;
    }

    levels_.clear();
    maxVolume_ = 0;
    for (int i = 0; i < numBins; ++i) {
        if (binCounts[i] == 0) continue;
        qreal price = minP + (i + 0.5) * binWidth;
        qreal avgVol = binVolumes[i];
        levels_.append({price, avgVol});
        maxVolume_ = std::max(maxVolume_, avgVol);
    }

    std::sort(levels_.begin(), levels_.end(), [](const PriceLevel& a, const PriceLevel& b) {
        return a.volume > b.volume;
    });

    update();
}

void VolumeProfileLayer::clear()
{
    levels_.clear();
    maxVolume_ = 0;
    update();
}

QSize VolumeProfileLayer::minimumSizeHint() const
{
    return QSize(80, 0);
}

void VolumeProfileLayer::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    p.fillRect(rect(), QColor(60, 60, 80));

    if (levels_.isEmpty()) return;

    qreal barArea = width() - 60.0;
    if (barArea < 10) barArea = 10;

    qreal barHeight = height() / static_cast<qreal>(levels_.size());
    if (barHeight < 1) barHeight = 1;

    QFont priceFont = font();
    priceFont.setPixelSize(9);
    p.setFont(priceFont);

    for (int i = 0; i < levels_.size(); ++i) {
        const auto& level = levels_[i];
        qreal y = i * barHeight;

        QRectF priceRect(0, y, 55, barHeight);
        p.setPen(QColor(180, 180, 200));
        p.drawText(priceRect, Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(level.price, 'f', 2));

        qreal barWidth = (level.volume / maxVolume_) * barArea;
        if (barWidth < 1) barWidth = 1;

        QRectF barRect(55, y + 1, barWidth, barHeight - 2);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(100, 180, 255, 200));
        p.drawRect(barRect);
    }
}

} // namespace fincept::screens
