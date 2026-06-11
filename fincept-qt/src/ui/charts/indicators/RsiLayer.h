#pragma once

#include "ui/charts/OverlayLayer.h"

class QLineSeries;
class QChart;
class QValueAxis;
class QGraphicsSimpleTextItem;

namespace fincept::ui {

class RsiLayer : public OverlayLayer {
    Q_OBJECT
public:
    explicit RsiLayer(const QString& id, const QString& name, int period = 14,
                      QObject* parent = nullptr);

    QString id() const override { return id_; }
    QString display_name() const override { return name_; }
    LayerType type() const override { return LayerType::Series; }

    void compute(const QVector<CandleData>& candles) override;
    void attach(QGraphicsScene* scene, QChart* chart) override;
    void detach(QGraphicsScene* scene, QChart* chart) override;
    void reposition(QChart* chart) override;

private:
    QString id_;
    QString name_;
    int period_;

    double last_value_ = 50;
    QLineSeries* series_ = nullptr;
    QLineSeries* overbought_ = nullptr;
    QLineSeries* oversold_ = nullptr;
};

} // namespace fincept::ui
