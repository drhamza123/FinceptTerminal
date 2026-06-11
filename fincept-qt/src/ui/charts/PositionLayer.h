#pragma once

#include "ui/charts/OverlayLayer.h"

#include <QColor>
#include <QVector>

class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsSimpleTextItem;
class QGraphicsPolygonItem;

namespace fincept::trading {
struct BrokerPosition;
}

namespace fincept::ui {

struct PositionLevel {
    QString symbol;
    QString side;
    double entry_price = 0;
    double stop_loss = 0;
    double take_profit = 0;
    double quantity = 0;
    double current_price = 0;
    double pnl = 0;

    QString order_id;
    bool is_open = true;
};

class PositionLayer : public OverlayLayer {
    Q_OBJECT
public:
    explicit PositionLayer(const QString& id, const QString& name, QObject* parent = nullptr);

    QString id() const override { return id_; }
    QString display_name() const override { return name_; }
    LayerType type() const override { return LayerType::HorizontalLine; }

    void compute(const QVector<CandleData>& candles) override;
    void attach(QGraphicsScene* scene, QChart* chart) override;
    void detach(QGraphicsScene* scene, QChart* chart) override;
    void reposition(QChart* chart) override;

    void set_positions(const QVector<PositionLevel>& positions);
    const QVector<PositionLevel>& positions() const { return positions_; }

private:
    void rebuild_items();
    void clear_items();

    QString id_;
    QString name_;
    QVector<PositionLevel> positions_;

    struct PosItem {
        QGraphicsLineItem* entry_line = nullptr;
        QGraphicsLineItem* sl_line = nullptr;
        QGraphicsLineItem* tp_line = nullptr;
        QGraphicsRectItem* entry_tag_bg = nullptr;
        QGraphicsSimpleTextItem* entry_tag_txt = nullptr;
        QGraphicsRectItem* sl_tag_bg = nullptr;
        QGraphicsSimpleTextItem* sl_tag_txt = nullptr;
        QGraphicsRectItem* tp_tag_bg = nullptr;
        QGraphicsSimpleTextItem* tp_tag_txt = nullptr;
        QGraphicsPolygonItem* marker = nullptr;
    };
    QVector<PosItem> items_;

    QGraphicsScene* scene_ = nullptr;
    QChart* chart_ = nullptr;
};

} // namespace fincept::ui
