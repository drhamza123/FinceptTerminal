#include "ui/drawing/DrawingManager.h"

#include <QChart>
#include <QGraphicsLineItem>
#include <QGraphicsScene>

namespace fincept::ui {

DrawingManager::DrawingManager(QObject* parent) : QObject(parent) {}

void DrawingManager::set_scene_chart(QGraphicsScene* scene, QChart* chart) {
    scene_ = scene;
    chart_ = chart;
}

void DrawingManager::set_active_tool(DrawingToolType type) {
    if (active_tool_ != type) {
        cancel_active();
        active_tool_ = type;
        emit tool_changed(type);
    }
}

void DrawingManager::cancel_active() {
    active_tool_ = DrawingToolType::None;
    pending_drawing_ = {};
    pending_points_.clear();
    if (preview_item_ && scene_) {
        scene_->removeItem(preview_item_);
        delete preview_item_;
        preview_item_ = nullptr;
    }
}

void DrawingManager::on_mouse_press(const QPointF& chart_point, const QPoint& view_point) {
    Q_UNUSED(view_point)
    if (!has_active_tool()) return;

    DrawingPoint dp = DrawingPoint::from_point(chart_point);
    pending_points_.append(chart_point);

    if (pending_points_.size() == 1) {
        pending_drawing_ = {};
        pending_drawing_.tool_type = active_tool_;
        pending_drawing_.points.append(dp);
        pending_drawing_.props.line_color = QColor("#d97706");
        pending_drawing_.created_at = QDateTime::currentMSecsSinceEpoch();
    }

    // Two-point tools complete on second click
    if (pending_points_.size() >= 2) {
        pending_drawing_.points.append(dp);
        QString id = add_drawing(pending_drawing_);
        if (!id.isEmpty()) {
            undo_stack_.push(new DrawingUndoCommand(
                DrawingUndoCommand::Add, {}, pending_drawing_));
        }
        cancel_active();
    }
}

void DrawingManager::on_mouse_move(const QPointF& chart_point, const QPoint& view_point) {
    Q_UNUSED(view_point)
    if (!has_active_tool() || pending_points_.isEmpty()) return;

    // Update preview
    if (preview_item_ && scene_) {
        scene_->removeItem(preview_item_);
        delete preview_item_;
        preview_item_ = nullptr;
    }

    // Draw preview line from first point to current mouse
    if (pending_points_.size() == 1 && chart_) {
        auto* line = scene_->addLine(
            QLineF(chart_->mapToPosition(pending_points_.first()),
                   chart_->mapToPosition(chart_point)),
            QPen(QColor("#d97706"), 1, Qt::DashLine));
        line->setZValue(100);
        preview_item_ = line;
    }
}

void DrawingManager::on_mouse_release(const QPointF& chart_point, const QPoint& view_point) {
    Q_UNUSED(chart_point)
    Q_UNUSED(view_point)
}

QString DrawingManager::add_drawing(const DrawingObject& obj) {
    DrawingObject copy = obj;
    copy.id = QString("draw_%1").arg(next_id_++);
    copy.modified_at = QDateTime::currentMSecsSinceEpoch();
    drawings_.append(copy);

    auto* item = create_item(copy);
    if (item) items_[copy.id] = item;

    emit drawing_added(copy.id);
    return copy.id;
}

void DrawingManager::remove_drawing(const QString& id) {
    for (int i = 0; i < drawings_.size(); ++i) {
        if (drawings_[i].id == id) {
            DrawingObject before = drawings_[i];
            drawings_.removeAt(i);
            remove_item(id);
            undo_stack_.push(new DrawingUndoCommand(
                DrawingUndoCommand::Remove, before, {}));
            emit drawing_removed(id);
            return;
        }
    }
}

void DrawingManager::clear_all() {
    for (const auto& id : items_.keys()) remove_item(id);
    drawings_.clear();
    items_.clear();
    undo_stack_.clear();
    cancel_active();
}

QGraphicsItem* DrawingManager::create_item(const DrawingObject& obj) {
    if (!scene_ || !chart_ || obj.points.size() < 2) return nullptr;

    // Map chart coords to screen coords
    QPointF p1 = chart_->mapToPosition(obj.points[0].to_point());
    QPointF p2 = chart_->mapToPosition(obj.points[1].to_point());

    QPen pen(obj.props.line_color, obj.props.line_width, obj.props.line_style);

    switch (obj.tool_type) {
    case DrawingToolType::TrendLine:
    case DrawingToolType::Ray:
    case DrawingToolType::ExtendedLine: {
        auto* line = scene_->addLine(QLineF(p1, p2), pen);
        line->setZValue(30);
        return line;
    }
    case DrawingToolType::HorizontalLine: {
        auto* line = scene_->addLine(
            QLineF(chart_->plotArea().left(), p1.y(),
                   chart_->plotArea().right(), p1.y()), pen);
        line->setZValue(30);
        return line;
    }
    case DrawingToolType::VerticalLine: {
        auto* line = scene_->addLine(
            QLineF(p1.x(), chart_->plotArea().top(),
                   p1.x(), chart_->plotArea().bottom()), pen);
        line->setZValue(30);
        return line;
    }
    case DrawingToolType::Channel: {
        if (obj.points.size() < 3) return nullptr;
        QPointF p3 = chart_->mapToPosition(obj.points[2].to_point());
        auto* channel = scene_->addLine(QLineF(p1, p2), pen);
        auto* channel2 = scene_->addLine(
            QLineF(p1 + (p3 - p2), p3), pen);
        pen.setStyle(Qt::DashLine);
        auto* connect = scene_->addLine(QLineF(p2, p3), pen);
        channel->setZValue(30);
        channel2->setZValue(30);
        connect->setZValue(30);
        // Store as a group using the channel property
        return channel; // simplified: just return first line
    }
    case DrawingToolType::TextLabel: {
        auto* txt = scene_->addSimpleText(obj.props.text);
        txt->setPos(p1);
        txt->setBrush(QBrush(obj.props.line_color));
        QFont f; f.setPixelSize(obj.props.font_size);
        txt->setFont(f);
        txt->setZValue(30);
        return txt;
    }
    default:
        return nullptr;
    }
}

void DrawingManager::remove_item(const QString& id) {
    auto it = items_.find(id);
    if (it != items_.end() && scene_) {
        scene_->removeItem(it.value());
        delete it.value();
        items_.erase(it);
    }
}

DrawingObject* DrawingManager::find_drawing(const QString& id) {
    for (auto& d : drawings_)
        if (d.id == id) return &d;
    return nullptr;
}

void DrawingManager::modify_drawing(const QString& id, const DrawingObject& obj) {
    for (auto& d : drawings_) {
        if (d.id == id) {
            DrawingObject before = d;
            d = obj;
            d.modified_at = QDateTime::currentMSecsSinceEpoch();
            update_item(id);
            undo_stack_.push(new DrawingUndoCommand(
                DrawingUndoCommand::Modify, before, d));
            emit drawing_modified(id);
            return;
        }
    }
}

void DrawingManager::update_item(const QString& id) {
    remove_item(id);
    auto* obj = find_drawing(id);
    if (obj) {
        auto* item = create_item(*obj);
        if (item) items_[id] = item;
    }
}

QPointF DrawingManager::snap_to_price(const QPointF& chart_pt, double threshold_pct) {
    Q_UNUSED(chart_pt)
    Q_UNUSED(threshold_pct)
    return chart_pt; // TODO: implement price snapping
}

QPointF DrawingManager::snap_to_ohlc(const QPointF& chart_pt, QVector<QPointF> ohlc_points) {
    Q_UNUSED(chart_pt)
    Q_UNUSED(ohlc_points)
    return chart_pt; // TODO: implement OHLC snapping
}

QPointF DrawingManager::snap_to_existing(const QPointF& chart_pt, double threshold) {
    Q_UNUSED(chart_pt)
    Q_UNUSED(threshold)
    return chart_pt; // TODO: implement snap to existing drawings
}

QByteArray DrawingManager::serialize() const {
    QJsonArray arr;
    for (const auto& d : drawings_) {
        QJsonObject o;
        o["id"] = d.id;
        o["tool_type"] = static_cast<int>(d.tool_type);
        QJsonArray pts;
        for (const auto& p : d.points) {
            QJsonObject pt;
            pt["price"] = p.price;
            pt["time"] = p.time_ms;
            pts.append(pt);
        }
        o["points"] = pts;
        o["color"] = d.props.line_color.name();
        o["width"] = d.props.line_width;
        o["style"] = static_cast<int>(d.props.line_style);
        o["text"] = d.props.text;
        arr.append(o);
    }
    return QJsonDocument(arr).toJson();
}

void DrawingManager::deserialize(const QByteArray& data) {
    clear_all();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;
    for (const auto& v : doc.array()) {
        QJsonObject o = v.toObject();
        DrawingObject d;
        d.id = o["id"].toString();
        d.tool_type = static_cast<DrawingToolType>(o["tool_type"].toInt());
        for (const auto& pv : o["points"].toArray()) {
            auto po = pv.toObject();
            DrawingPoint pt;
            pt.price = po["price"].toDouble();
            pt.time_ms = static_cast<qint64>(po["time"].toDouble());
            d.points.append(pt);
        }
        d.props.line_color = QColor(o["color"].toString());
        d.props.line_width = o["width"].toDouble();
        d.props.line_style = static_cast<Qt::PenStyle>(o["style"].toInt());
        d.props.text = o["text"].toString();

        auto* item = create_item(d);
        if (item) { items_[d.id] = item; drawings_.append(d); }
    }
}

} // namespace fincept::ui
