#include "ui/charts/PositionLayer.h"

#include <QChart>
#include <QGraphicsLineItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPen>
#include <QPolygonF>

namespace fincept::ui {

PositionLayer::PositionLayer(const QString& id, const QString& name, QObject* parent)
    : OverlayLayer(parent), id_(id), name_(name) {}

void PositionLayer::compute(const QVector<CandleData>&) {}

void PositionLayer::set_positions(const QVector<PositionLevel>& positions) {
    positions_ = positions;
    if (scene_ && chart_) {
        clear_items();
        rebuild_items();
        reposition(chart_);
    }
}

void PositionLayer::attach(QGraphicsScene* scene, QChart* chart) {
    scene_ = scene;
    chart_ = chart;
    rebuild_items();
    reposition(chart);
}

void PositionLayer::detach(QGraphicsScene* scene, QChart*) {
    clear_items();
    scene_ = nullptr;
    chart_ = nullptr;
}

void PositionLayer::reposition(QChart* chart) {
    if (!chart || items_.isEmpty())
        return;

    const QRectF plot = chart->plotArea();
    constexpr qreal pad_x = 4, pad_y = 1;

    QFont tag_font;
    tag_font.setFamily(QStringLiteral("Consolas"));
    tag_font.setPixelSize(10);
    tag_font.setWeight(QFont::DemiBold);

    for (int i = 0; i < items_.size() && i < positions_.size(); ++i) {
        const auto& pos = positions_[i];
        const QPointF entry_pt = chart->mapToPosition(QPointF(0, pos.entry_price));
        const qreal ey = entry_pt.y();
        const bool in_view = ey >= plot.top() && ey <= plot.bottom();

        double sl_y = 0, tp_y = 0;
        bool sl_vis = false, tp_vis = false;
        if (pos.stop_loss > 0) {
            sl_y = chart->mapToPosition(QPointF(0, pos.stop_loss)).y();
            sl_vis = sl_y >= plot.top() && sl_y <= plot.bottom();
        }
        if (pos.take_profit > 0) {
            tp_y = chart->mapToPosition(QPointF(0, pos.take_profit)).y();
            tp_vis = tp_y >= plot.top() && tp_y <= plot.bottom();
        }

        items_[i].entry_line->setLine(plot.left(), ey, plot.right(), ey);
        items_[i].entry_line->setVisible(in_view && visible());

        if (items_[i].sl_line) {
            items_[i].sl_line->setLine(plot.left(), sl_y, plot.right(), sl_y);
            items_[i].sl_line->setVisible(sl_vis && visible());
        }
        if (items_[i].tp_line) {
            items_[i].tp_line->setLine(plot.left(), tp_y, plot.right(), tp_y);
            items_[i].tp_line->setVisible(tp_vis && visible());
        }

        auto position_tag = [&](QGraphicsRectItem* bg, QGraphicsSimpleTextItem* txt,
                                 qreal y, bool vis) {
            if (!bg || !txt) return;
            const QRectF tb = txt->boundingRect();
            const qreal tw = tb.width() + 2 * pad_x;
            const qreal th = tb.height() + 2 * pad_y;
            bg->setRect(QRectF(plot.right() + 2, y - th / 2.0, tw, th));
            txt->setPos(plot.right() + 2 + pad_x, y - th / 2.0 + pad_y);
            bg->setVisible(vis && visible());
            txt->setVisible(vis && visible());
        };

        position_tag(items_[i].entry_tag_bg, items_[i].entry_tag_txt, ey, in_view);
        position_tag(items_[i].sl_tag_bg, items_[i].sl_tag_txt, sl_y, sl_vis);
        position_tag(items_[i].tp_tag_bg, items_[i].tp_tag_txt, tp_y, tp_vis);

        if (items_[i].marker) {
            const qreal marker_x = plot.left() + 10;
            items_[i].marker->setPos(marker_x, ey);
            items_[i].marker->setVisible(in_view && visible());
        }
    }
}

void PositionLayer::rebuild_items() {
    if (!scene_)
        return;

    QFont tag_font;
    tag_font.setFamily(QStringLiteral("Consolas"));
    tag_font.setPixelSize(10);
    tag_font.setWeight(QFont::DemiBold);

    for (const auto& pos : positions_) {
        PosItem item;
        const bool is_long = (pos.side == "long" || pos.side == "buy");
        const QColor entry_color = is_long ? QColor("#089981") : QColor("#f23645");
        const QColor sl_color("#f23645");
        const QColor tp_color("#089981");

        // Entry line
        QPen entry_pen(entry_color);
        entry_pen.setStyle(Qt::SolidLine);
        entry_pen.setWidth(2);
        item.entry_line = scene_->addLine(QLineF(), entry_pen);
        item.entry_line->setZValue(8);

        // SL line
        QPen sl_pen(sl_color);
        sl_pen.setStyle(Qt::DashLine);
        sl_pen.setWidth(1);
        item.sl_line = scene_->addLine(QLineF(), sl_pen);
        item.sl_line->setZValue(8);

        // TP line
        QPen tp_pen(tp_color);
        tp_pen.setStyle(Qt::DashLine);
        tp_pen.setWidth(1);
        item.tp_line = scene_->addLine(QLineF(), tp_pen);
        item.tp_line->setZValue(8);

        // Entry tag
        QColor entry_bg = entry_color;
        entry_bg.setAlpha(200);
        item.entry_tag_bg = scene_->addRect(QRectF(), QPen(Qt::NoPen), QBrush(entry_bg));
        item.entry_tag_bg->setZValue(21);
        QString entry_label = QString("%1 %2").arg(pos.side == "long" || pos.side == "buy" ? "L" : "S")
                                               .arg(pos.entry_price, 0, 'f', 2);
        item.entry_tag_txt = scene_->addSimpleText(entry_label);
        item.entry_tag_txt->setBrush(QBrush(Qt::white));
        item.entry_tag_txt->setFont(tag_font);
        item.entry_tag_txt->setZValue(22);

        // SL tag
        if (pos.stop_loss > 0) {
            QColor sl_bg = sl_color;
            sl_bg.setAlpha(200);
            item.sl_tag_bg = scene_->addRect(QRectF(), QPen(Qt::NoPen), QBrush(sl_bg));
            item.sl_tag_bg->setZValue(21);
            QString sl_label = QString("SL %1").arg(pos.stop_loss, 0, 'f', 2);
            item.sl_tag_txt = scene_->addSimpleText(sl_label);
            item.sl_tag_txt->setBrush(QBrush(Qt::white));
            item.sl_tag_txt->setFont(tag_font);
            item.sl_tag_txt->setZValue(22);
        }

        // TP tag
        if (pos.take_profit > 0) {
            QColor tp_bg = tp_color;
            tp_bg.setAlpha(200);
            item.tp_tag_bg = scene_->addRect(QRectF(), QPen(Qt::NoPen), QBrush(tp_bg));
            item.tp_tag_bg->setZValue(21);
            QString tp_label = QString("TP %1").arg(pos.take_profit, 0, 'f', 2);
            item.tp_tag_txt = scene_->addSimpleText(tp_label);
            item.tp_tag_txt->setBrush(QBrush(Qt::white));
            item.tp_tag_txt->setFont(tag_font);
            item.tp_tag_txt->setZValue(22);
        }

        // Entry marker arrow
        QPolygonF arrow;
        if (is_long) {
            arrow << QPointF(0, -6) << QPointF(10, 0) << QPointF(0, 6);
        } else {
            arrow << QPointF(0, 6) << QPointF(10, 0) << QPointF(0, -6);
        }
        item.marker = scene_->addPolygon(arrow, QPen(Qt::NoPen), QBrush(entry_color));
        item.marker->setZValue(9);

        items_.append(item);
    }
}

void PositionLayer::clear_items() {
    auto remove = [&](auto*& item) {
        if (item && scene_) {
            scene_->removeItem(item);
            delete item;
            item = nullptr;
        }
    };
    for (auto& pi : items_) {
        remove(pi.entry_line);
        remove(pi.sl_line);
        remove(pi.tp_line);
        remove(pi.entry_tag_bg);
        remove(pi.entry_tag_txt);
        remove(pi.sl_tag_bg);
        remove(pi.sl_tag_txt);
        remove(pi.tp_tag_bg);
        remove(pi.tp_tag_txt);
        remove(pi.marker);
    }
    items_.clear();
}

int PositionLayer::hit_test(const QPointF& view_pos) const {
    if (!chart_) return -1;
    const qreal threshold = 6; // pixels
    for (int i = 0; i < items_.size() && i < positions_.size(); ++i) {
        auto check_line = [&](QGraphicsLineItem* line, qreal& out_y) -> bool {
            if (!line || !line->isVisible()) return false;
            QLineF l = line->line();
            qreal dy = std::abs(view_pos.y() - l.y1());
            if (dy < threshold && view_pos.x() >= l.x1() && view_pos.x() <= l.x2()) {
                out_y = l.y1();
                return true;
            }
            return false;
        };
        qreal y;
        if (check_line(items_[i].sl_line, y)) { drag_target_ = SLLine; drag_index_ = i; return i; }
        if (check_line(items_[i].tp_line, y)) { drag_target_ = TPPLine; drag_index_ = i; return i; }
        if (check_line(items_[i].entry_line, y)) { drag_target_ = EntryLine; drag_index_ = i; return i; }
    }
    return -1;
}

void PositionLayer::on_mouse_press(const QPointF& chart_pos, const QPoint& view_pos) {
    Q_UNUSED(chart_pos)
    drag_active_ = false;
    drag_target_ = None;
    drag_index_ = -1;

    if (hit_test(view_pos) >= 0) {
        drag_active_ = true;
        drag_start_y_ = view_pos.y();
    }
}

void PositionLayer::on_mouse_move(const QPointF& chart_pos) {
    if (!drag_active_ || !chart_ || drag_index_ < 0 || drag_index_ >= positions_.size())
        return;

    auto set_line_y = [&](QGraphicsLineItem* line, qreal y) {
        if (!line) return;
        QRectF plot = chart_->plotArea();
        line->setLine(plot.left(), y, plot.right(), y);
    };

    qreal new_y = chart_->mapToPosition(chart_pos).y();

    switch (drag_target_) {
    case SLLine:
        set_line_y(items_[drag_index_].sl_line, new_y);
        break;
    case TPPLine:
        set_line_y(items_[drag_index_].tp_line, new_y);
        break;
    case EntryLine:
        set_line_y(items_[drag_index_].entry_line, new_y);
        break;
    default:
        break;
    }
}

void PositionLayer::on_mouse_release() {
    if (!drag_active_ || !chart_ || drag_index_ < 0 || drag_index_ >= positions_.size()) {
        drag_active_ = false;
        return;
    }

    // Map screen Y back to price
    auto line_y_to_price = [&](QGraphicsLineItem* line) -> double {
        if (!line) return 0;
        qreal y = line->line().y1();
        return chart_->mapToValue(QPointF(0, y)).y();
    };

    double new_sl = positions_[drag_index_].stop_loss;
    double new_tp = positions_[drag_index_].take_profit;

    if (drag_target_ == SLLine) new_sl = line_y_to_price(items_[drag_index_].sl_line);
    if (drag_target_ == TPPLine) new_tp = line_y_to_price(items_[drag_index_].tp_line);

    // Update the stored position
    positions_[drag_index_].stop_loss = new_sl;
    positions_[drag_index_].take_profit = new_tp;

    emit sl_tp_changed(positions_[drag_index_].order_id, new_sl, new_tp);

    // Full reposition to snap positions
    reposition(chart_);
    drag_active_ = false;
}

} // namespace fincept::ui
