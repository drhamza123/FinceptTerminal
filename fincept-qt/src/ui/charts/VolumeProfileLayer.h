#pragma once

#include "ui/charts/OverlayLayer.h"

#include <QColor>
#include <QVector>

class QGraphicsRectItem;
class QGraphicsLineItem;

namespace fincept::ui {

class VolumeProfileLayer : public OverlayLayer {
    Q_OBJECT
public:
    explicit VolumeProfileLayer(const QString& id, const QString& name,
                                int num_bins = 50, QObject* parent = nullptr);

    QString id() const override { return id_; }
    QString display_name() const override { return name_; }
    LayerType type() const override { return LayerType::Annotation; }

    void compute(const QVector<CandleData>& candles) override;
    void attach(QGraphicsScene* scene, QChart* chart) override;
    void detach(QGraphicsScene* scene, QChart* chart) override;
    void reposition(QChart* chart) override;

    void set_num_bins(int bins) { num_bins_ = bins; recompute_ = true; }

private:
    QString id_;
    QString name_;
    int num_bins_;
    bool recompute_ = true;

    struct BinData {
        double price;
        double volume;
        double norm_volume; // 0..1
    };
    QVector<BinData> bins_;
    double poc_price_ = 0;
    double vah_ = 0, val_ = 0;
    double max_volume_ = 0;

    struct BarItem {
        QGraphicsRectItem* bar = nullptr;
        QGraphicsLineItem* poc_line = nullptr;
        QGraphicsLineItem* va_line_top = nullptr;
        QGraphicsLineItem* va_line_bot = nullptr;
    };
    BarItem items_;
};

} // namespace fincept::ui
