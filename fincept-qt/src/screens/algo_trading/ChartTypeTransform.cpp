#include "screens/algo_trading/ChartTypeTransform.h"
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace fincept::screens {

double ChartTypeTransform::calculateATR(const QVector<OhlcvPoint>& data, int period) {
    if (data.size() < period + 1) return 0.0;

    double sumTr = 0.0;
    for (int i = 1; i <= period; ++i) {
        const auto& prev = data[i - 1];
        const auto& cur = data[i];
        double tr = cur.high - cur.low;
        tr = std::max(tr, std::abs(cur.high - prev.close));
        tr = std::max(tr, std::abs(cur.low - prev.close));
        sumTr += tr;
    }
    double atr = sumTr / period;

    for (int i = period + 1; i < data.size(); ++i) {
        const auto& prev = data[i - 1];
        const auto& cur = data[i];
        double tr = cur.high - cur.low;
        tr = std::max(tr, std::abs(cur.high - prev.close));
        tr = std::max(tr, std::abs(cur.low - prev.close));
        atr = (atr * (period - 1) + tr) / period;
    }

    return atr;
}

QVector<OhlcvPoint> ChartTypeTransform::toHeikinAshi(const QVector<OhlcvPoint>& data) {
    if (data.isEmpty()) return {};

    QVector<OhlcvPoint> result;
    result.reserve(data.size());

    for (const auto& pt : data) {
        OhlcvPoint ha;
        ha.time = pt.time;
        ha.volume = pt.volume;
        ha.close = (pt.open + pt.high + pt.low + pt.close) / 4.0;

        if (result.isEmpty()) {
            ha.open = (pt.open + pt.close) / 2.0;
        } else {
            ha.open = (result.last().open + result.last().close) / 2.0;
        }

        ha.high = std::max({pt.high, ha.open, ha.close});
        ha.low = std::min({pt.low, ha.open, ha.close});

        result.append(ha);
    }

    return result;
}

QVector<OhlcvPoint> ChartTypeTransform::toRenko(const QVector<OhlcvPoint>& data, double brickSize) {
    if (data.isEmpty()) return {};
    if (brickSize <= 0.0) brickSize = calculateATR(data, 14);
    if (brickSize <= 0.0) return data;

    QVector<OhlcvPoint> result;
    double level = std::floor(data[0].close / brickSize) * brickSize;
    if (level == 0.0) level = data[0].close;

    double accVol = 0.0;

    for (const auto& pt : data) {
        accVol += pt.volume;

        int numBricks = 0;
        double direction = 0.0;

        if (pt.close >= level + brickSize) {
            numBricks = static_cast<int>((pt.close - level) / brickSize);
            direction = 1.0;
        } else if (pt.close <= level - brickSize) {
            numBricks = static_cast<int>((level - pt.close) / brickSize);
            direction = -1.0;
        }

        for (int i = 0; i < numBricks; ++i) {
            OhlcvPoint brick;
            brick.time = pt.time;
            brick.open = level;
            level += direction * brickSize;
            brick.close = level;
            brick.volume = accVol / numBricks;

            if (direction > 0.0) {
                brick.high = level;
                brick.low = level - brickSize;
            } else {
                brick.high = level + brickSize;
                brick.low = level;
            }

            result.append(brick);
        }

        if (numBricks > 0) accVol = 0.0;
    }

    return result;
}

QVector<OhlcvPoint> ChartTypeTransform::toKagi(const QVector<OhlcvPoint>& data, double reversalAmount) {
    if (data.isEmpty()) return {};
    if (reversalAmount <= 0.0) reversalAmount = calculateATR(data, 14);
    if (reversalAmount <= 0.0) return data;

    QVector<OhlcvPoint> result;

    double anchor = data[0].close;
    double price = data[0].close;
    bool directionUp = false;
    bool directionSet = false;
    qreal lastTime = data[0].time;
    double accVol = 0.0;

    for (const auto& pt : data) {
        accVol += pt.volume;
        lastTime = pt.time;

        if (!directionSet) {
            double move = pt.close - anchor;
            if (std::abs(move) >= reversalAmount) {
                directionUp = move > 0;
                directionSet = true;
                price = pt.close;
            }
            continue;
        }

        if (directionUp) {
            if (pt.close >= price) {
                price = pt.close;
            } else if (pt.close <= price - reversalAmount) {
                double reversalPrice = price - reversalAmount;
                OhlcvPoint seg;
                seg.time = lastTime;
                seg.open = price;
                seg.close = reversalPrice;
                seg.high = std::max(price, reversalPrice);
                seg.low = std::min(price, reversalPrice);
                seg.volume = accVol;
                result.append(seg);
                accVol = 0.0;

                price = reversalPrice;
                directionUp = false;

                if (pt.close < price) {
                    price = pt.close;
                }
            }
        } else {
            if (pt.close <= price) {
                price = pt.close;
            } else if (pt.close >= price + reversalAmount) {
                double reversalPrice = price + reversalAmount;
                OhlcvPoint seg;
                seg.time = lastTime;
                seg.open = price;
                seg.close = reversalPrice;
                seg.high = std::max(price, reversalPrice);
                seg.low = std::min(price, reversalPrice);
                seg.volume = accVol;
                result.append(seg);
                accVol = 0.0;

                price = reversalPrice;
                directionUp = true;

                if (pt.close > price) {
                    price = pt.close;
                }
            }
        }
    }

    return result;
}

QVector<OhlcvPoint> ChartTypeTransform::toPointFigure(const QVector<OhlcvPoint>& data, double boxSize, double reversal) {
    if (data.isEmpty()) return {};
    if (boxSize <= 0.0) boxSize = calculateATR(data, 14);
    if (boxSize <= 0.0) return data;

    QVector<OhlcvPoint> result;
    bool rising = true;
    double currentTop = std::ceil(data[0].close / boxSize) * boxSize;
    double currentBottom = currentTop - boxSize;
    double colHigh = currentTop;
    double colLow = currentBottom;
    double accVol = 0.0;
    qreal colTime = data[0].time;

    for (const auto& pt : data) {
        accVol += pt.volume;
        colTime = pt.time;

        if (rising) {
            if (pt.close >= currentTop + boxSize) {
                int steps = static_cast<int>((pt.close - currentTop) / boxSize);
                currentTop += steps * boxSize;
                colHigh = currentTop;
            } else if (pt.close <= currentTop - reversal * boxSize) {
                double newBottom = std::floor(pt.close / boxSize) * boxSize;
                OhlcvPoint col;
                col.time = colTime;
                col.open = colLow;
                col.high = colHigh;
                col.low = colLow;
                col.close = colHigh;
                col.volume = accVol;
                result.append(col);
                accVol = 0.0;

                rising = false;
                currentBottom = newBottom;
                currentTop = currentBottom + boxSize;
                colLow = currentBottom;
                colHigh = currentTop;
            }
        } else {
            if (pt.close <= currentBottom - boxSize) {
                int steps = static_cast<int>((currentBottom - pt.close) / boxSize);
                currentBottom -= steps * boxSize;
                colLow = currentBottom;
            } else if (pt.close >= currentBottom + reversal * boxSize) {
                double newTop = std::ceil(pt.close / boxSize) * boxSize;
                OhlcvPoint col;
                col.time = colTime;
                col.open = colLow;
                col.high = colHigh;
                col.low = colLow;
                col.close = colHigh;
                col.volume = accVol;
                result.append(col);
                accVol = 0.0;

                rising = true;
                currentTop = newTop;
                currentBottom = currentTop - boxSize;
                colHigh = currentTop;
                colLow = currentBottom;
            }
        }
    }

    if (accVol > 0.0) {
        OhlcvPoint col;
        col.time = colTime;
        col.open = colLow;
        col.high = colHigh;
        col.low = colLow;
        col.close = colHigh;
        col.volume = accVol;
        result.append(col);
    }

    return result;
}

QVector<OhlcvPoint> ChartTypeTransform::toRangeBars(const QVector<OhlcvPoint>& data, double rangeSize) {
    if (data.isEmpty()) return {};
    if (rangeSize <= 0.0) rangeSize = calculateATR(data, 14);
    if (rangeSize <= 0.0) return data;

    QVector<OhlcvPoint> result;
    double barOpen = data[0].close;
    double accVol = 0.0;

    for (const auto& pt : data) {
        accVol += pt.volume;
        double typicalPrice = (pt.high + pt.low + pt.close) / 3.0;
        double upwardMove = typicalPrice - barOpen;
        double downwardMove = barOpen - typicalPrice;

        if (upwardMove >= rangeSize) {
            int numBars = static_cast<int>(upwardMove / rangeSize);
            for (int i = 0; i < numBars; ++i) {
                OhlcvPoint bar;
                bar.time = pt.time;
                bar.open = barOpen;
                bar.high = barOpen + rangeSize;
                bar.low = barOpen;
                bar.close = barOpen + rangeSize;
                bar.volume = accVol / numBars;
                result.append(bar);
                barOpen += rangeSize;
            }
            accVol = 0.0;
        } else if (downwardMove >= rangeSize) {
            int numBars = static_cast<int>(downwardMove / rangeSize);
            for (int i = 0; i < numBars; ++i) {
                OhlcvPoint bar;
                bar.time = pt.time;
                bar.open = barOpen;
                bar.high = barOpen;
                bar.low = barOpen - rangeSize;
                bar.close = barOpen - rangeSize;
                bar.volume = accVol / numBars;
                result.append(bar);
                barOpen -= rangeSize;
            }
            accVol = 0.0;
        }
    }

    return result;
}

QStringList ChartTypeTransform::availableTransforms() {
    return {"None", "Heikin-Ashi", "Renko", "Kagi", "Point & Figure", "Range Bars"};
}

QVector<OhlcvPoint> ChartTypeTransform::transform(const QString& type, const QVector<OhlcvPoint>& data) {
    if (type == "Heikin-Ashi") return toHeikinAshi(data);
    if (type == "Renko") return toRenko(data);
    if (type == "Kagi") return toKagi(data);
    if (type == "Point & Figure") return toPointFigure(data);
    if (type == "Range Bars") return toRangeBars(data);
    return data;
}

} // namespace fincept::screens
