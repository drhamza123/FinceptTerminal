#pragma once

#include "ui/drawing/DrawingTypes.h"

#include <QObject>
#include <QPointF>
#include <QUndoStack>
#include <QVector>

class QChart;
class QGraphicsScene;
class QGraphicsItem;
class QGraphicsLineItem;
class QGraphicsPolygonItem;
class QGraphicsEllipseItem;
class QGraphicsSimpleTextItem;

namespace fincept::ui {

class DrawingManager : public QObject {
    Q_OBJECT
public:
    explicit DrawingManager(QObject* parent = nullptr);

    void set_scene_chart(QGraphicsScene* scene, QChart* chart);
    QChart* chart() const { return chart_; }

    // Tool management
    void set_active_tool(DrawingToolType type);
    DrawingToolType active_tool() const { return active_tool_; }
    bool has_active_tool() const { return active_tool_ != DrawingToolType::None; }

    // Drawing management
    QString add_drawing(const DrawingObject& obj);
    void remove_drawing(const QString& id);
    void modify_drawing(const QString& id, const DrawingObject& obj);
    DrawingObject* find_drawing(const QString& id);
    QVector<DrawingObject>& all_drawings() { return drawings_; }

    // Interaction
    void on_mouse_press(const QPointF& chart_point, const QPoint& view_point);
    void on_mouse_move(const QPointF& chart_point, const QPoint& view_point);
    void on_mouse_release(const QPointF& chart_point, const QPoint& view_point);
    void cancel_active();

    // Snap
    QPointF snap_to_price(const QPointF& chart_pt, double threshold_pct = 0.003);
    QPointF snap_to_ohlc(const QPointF& chart_pt, QVector<QPointF> ohlc_points);
    QPointF snap_to_existing(const QPointF& chart_pt, double threshold = 5.0);

    // Undo/Redo
    QUndoStack* undo_stack() { return &undo_stack_; }
    void clear_all();

    // Serialization
    QByteArray serialize() const;
    void deserialize(const QByteArray& data);

signals:
    void drawing_added(const QString& id);
    void drawing_removed(const QString& id);
    void drawing_modified(const QString& id);
    void tool_changed(DrawingToolType type);

private:
    QGraphicsItem* create_item(const DrawingObject& obj);
    void update_item(const QString& id);
    void remove_item(const QString& id);
    void place_drawing(const QPointF& pt);

    // Tool placement state
    DrawingToolType active_tool_ = DrawingToolType::None;
    DrawingObject pending_drawing_;
    QVector<QPointF> pending_points_;
    QGraphicsItem* preview_item_ = nullptr;

    QGraphicsScene* scene_ = nullptr;
    QChart* chart_ = nullptr;
    QVector<DrawingObject> drawings_;
    QHash<QString, QGraphicsItem*> items_;
    int next_id_ = 1;
    QUndoStack undo_stack_;

    static constexpr int MAX_SNAP_DIST_PX = 8;
};

} // namespace fincept::ui
