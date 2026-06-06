// ChartTypeTransform.h — Alternative chart type transformations
#pragma once
#include <QVector>
#include <QString>
#include <QStringList>

namespace fincept::screens {

struct OhlcvPoint {
    qreal time = 0, open = 0, high = 0, low = 0, close = 0, volume = 0;
};

class ChartTypeTransform {
public:
    static QVector<OhlcvPoint> toHeikinAshi(const QVector<OhlcvPoint>& data);
    static QVector<OhlcvPoint> toRenko(const QVector<OhlcvPoint>& data, double brickSize = 0.0);
    static QVector<OhlcvPoint> toKagi(const QVector<OhlcvPoint>& data, double reversalAmount = 0.0);
    static QVector<OhlcvPoint> toPointFigure(const QVector<OhlcvPoint>& data, double boxSize = 0.0, double reversal = 3.0);
    static QVector<OhlcvPoint> toRangeBars(const QVector<OhlcvPoint>& data, double rangeSize = 0.0);

    static double calculateATR(const QVector<OhlcvPoint>& data, int period = 14);
    static QStringList availableTransforms();
    static QVector<OhlcvPoint> transform(const QString& type, const QVector<OhlcvPoint>& data);

private:
    ChartTypeTransform() = delete;
};

} // namespace fincept::screens
