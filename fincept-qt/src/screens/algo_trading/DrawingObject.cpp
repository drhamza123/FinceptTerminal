// DrawingObject.cpp — Interactive analytical drawing objects
#include "screens/algo_trading/DrawingObject.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QStyleOptionGraphicsItem>
#include <QJsonArray>
#include <QtMath>
#include <QCursor>

namespace fincept::screens {

// ── DrawingObject Base ──────────────────────────────────────────

DrawingObject::DrawingObject(DrawingToolType toolType, const QColor& color,
                             qreal width, QGraphicsItem* parent)
    : QGraphicsObject(parent), toolType_(toolType) {
    pen_ = QPen(color, width);
    pen_.setCosmetic(true);
    setAcceptHoverEvents(true);
    setFlags(ItemIsSelectable | ItemSendsGeometryChanges);
    id_ = QString("draw_%1").arg(reinterpret_cast<quintptr>(this), 0, 16);
}

DrawingObject::~DrawingObject() = default;

QString DrawingObject::toolName() const {
    switch (toolType_) {
        case DrawingToolType::TrendLine: return "TrendLine";
        case DrawingToolType::HorizontalLine: return "H-Line";
        case DrawingToolType::VerticalLine: return "V-Line";
        case DrawingToolType::Channel: return "Channel";
        case DrawingToolType::Ray: return "Ray";
        case DrawingToolType::FibonacciRetrace: return "Fib Retrace";
        case DrawingToolType::FibonacciExtension: return "Fib Extension";
        case DrawingToolType::FibonacciFan: return "Fib Fan";
        case DrawingToolType::FibonacciArc: return "Fib Arc";
        case DrawingToolType::GannFan: return "Gann Fan";
        case DrawingToolType::GannSquare: return "Gann Square";
        case DrawingToolType::ElliottWave: return "Elliott Wave";
        case DrawingToolType::AndrewsPitchfork: return "Pitchfork";
        case DrawingToolType::CycleLine: return "Cycle Line";
        case DrawingToolType::Crosshair: return "Crosshair";
        case DrawingToolType::Brush: return "Brush";
        case DrawingToolType::Measure: return "Measure";
        case DrawingToolType::TextLabel: return "Text";
        default: return "None";
    }
}

void DrawingObject::setColor(const QColor& c) { pen_.setColor(c); update(); }
void DrawingObject::setLineWidth(qreal w) { pen_.setWidthF(w); update(); }
void DrawingObject::setLocked(bool locked) { locked_ = locked; setFlag(ItemIsSelectable, !locked); }
void DrawingObject::setVisible(bool v) { visible_ = v; QGraphicsObject::setVisible(v); }

QJsonObject DrawingObject::toJson() const {
    QJsonObject obj;
    obj["id"] = id_;
    obj["tool"] = toolName();
    obj["color"] = pen_.color().name();
    obj["width"] = pen_.widthF();
    obj["locked"] = locked_;
    return obj;
}

void DrawingObject::fromJson(const QJsonObject& obj) {
    id_ = obj["id"].toString();
    if (obj.contains("color")) setColor(QColor(obj["color"].toString()));
    if (obj.contains("width")) setLineWidth(obj["width"].toDouble());
    if (obj.contains("locked")) locked_ = obj["locked"].toBool();
}

void DrawingObject::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    if (!locked_) { setCursor(Qt::SizeAllCursor); }
    QGraphicsObject::hoverEnterEvent(event);
}
void DrawingObject::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}
void DrawingObject::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (locked_) { event->ignore(); return; }
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        dragStart_ = event->scenePos();
        dragOrigin_ = pos();
        emit objectSelected(this);
    }
    QGraphicsObject::mousePressEvent(event);
}
void DrawingObject::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (!dragging_ || locked_) { QGraphicsObject::mouseMoveEvent(event); return; }
    QPointF delta = event->scenePos() - dragStart_;
    setPos(dragOrigin_ + delta);
    emit objectModified();
}
void DrawingObject::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    dragging_ = false;
    if (pos() != dragOrigin_) emit objectModified();
    QGraphicsObject::mouseReleaseEvent(event);
}
QVariant DrawingObject::itemChange(GraphicsItemChange change, const QVariant& value) {
    return QGraphicsObject::itemChange(change, value);
}

// ── TrendLineObject ─────────────────────────────────────────────

TrendLineObject::TrendLineObject(const QPointF& start, const QPointF& end,
                                 const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::TrendLine, color, 1.5, parent), start_(start), end_(end) {
    updateBounds();
}

void TrendLineObject::setPoints(const QPointF& start, const QPointF& end) {
    prepareGeometryChange();
    start_ = start; end_ = end;
    updateBounds();
    emit objectModified();
}

void TrendLineObject::updateBounds() {
    qreal l = qMin(start_.x(), end_.x()) - 10;
    qreal t = qMin(start_.y(), end_.y()) - 10;
    qreal r = qMax(start_.x(), end_.x()) + 10;
    qreal b = qMax(start_.y(), end_.y()) + 10;
    bounds_ = QRectF(l, t, r-l, b-t);
}

QRectF TrendLineObject::boundingRect() const { return bounds_; }

void TrendLineObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen_);
    painter->drawLine(start_, end_);
    if (isSelected()) {
        painter->setPen(QPen(QColor(255,255,255), 1));
        painter->setBrush(Qt::white);
        painter->drawEllipse(start_, CP_RADIUS, CP_RADIUS);
        painter->drawEllipse(end_, CP_RADIUS, CP_RADIUS);
    }
}

int TrendLineObject::controlPointAt(const QPointF& scenePos) const {
    if (QLineF(scenePos, start_).length() < CP_RADIUS*2) return 0;
    if (QLineF(scenePos, end_).length() < CP_RADIUS*2) return 1;
    return -1;
}

void TrendLineObject::moveControlPoint(int index, const QPointF& newPos) {
    prepareGeometryChange();
    if (index == 0) start_ = newPos;
    else if (index == 1) end_ = newPos;
    updateBounds();
    emit objectModified();
}

QJsonObject TrendLineObject::toJson() const {
    auto obj = DrawingObject::toJson();
    obj["start"] = QJsonArray({start_.x(), start_.y()});
    obj["end"] = QJsonArray({end_.x(), end_.y()});
    return obj;
}

void TrendLineObject::fromJson(const QJsonObject& obj) {
    DrawingObject::fromJson(obj);
    auto s = obj["start"].toArray();
    auto e = obj["end"].toArray();
    if (s.size()==2 && e.size()==2) setPoints(QPointF(s[0].toDouble(), s[1].toDouble()),
                                                QPointF(e[0].toDouble(), e[1].toDouble()));
}

// ── HorizontalLineObject ───────────────────────────────────────

HorizontalLineObject::HorizontalLineObject(qreal y, qreal left, qreal right,
                                           const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::HorizontalLine, color, 1.2, parent),
      y_(y), left_(left), right_(right) {}

QRectF HorizontalLineObject::boundingRect() const {
    return QRectF(left_, y_-8, right_-left_, 16);
}

void HorizontalLineObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen_);
    painter->drawLine(QPointF(left_, y_), QPointF(right_, y_));
    painter->setFont(textFont_);
    painter->setPen(textColor_);
    painter->drawText(QPointF(right_ - 60, y_ - 4), QString::number(y_, 'f', 2));
    if (isSelected()) {
        painter->setPen(QPen(Qt::white, 1));
        painter->setBrush(Qt::white);
        painter->drawEllipse(QPointF(left_, y_), 5, 5);
        painter->drawEllipse(QPointF(right_, y_), 5, 5);
    }
}

void HorizontalLineObject::setYValue(qreal y) { prepareGeometryChange(); y_ = y; update(); emit objectModified(); }
int HorizontalLineObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, QPointF(left_, y_)).length() < 6) return 0;
    if (QLineF(p, QPointF(right_, y_)).length() < 6) return 1;
    return -1;
}
void HorizontalLineObject::moveControlPoint(int idx, const QPointF& p) {
    if (idx == 0) { left_ = p.x(); }
    else if (idx == 1) { right_ = p.x(); }
    y_ = p.y();
    emit objectModified();
}
QJsonObject HorizontalLineObject::toJson() const { auto o=DrawingObject::toJson(); o["y"]=y_; o["left"]=left_; o["right"]=right_; return o; }
void HorizontalLineObject::fromJson(const QJsonObject& o) { DrawingObject::fromJson(o); y_=o["y"].toDouble(); left_=o["left"].toDouble(); right_=o["right"].toDouble(); }

// ── VerticalLineObject ─────────────────────────────────────────

VerticalLineObject::VerticalLineObject(qreal x, qreal top, qreal bottom,
                                       const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::VerticalLine, color, 1.2, parent),
      x_(x), top_(top), bottom_(bottom) {}
QRectF VerticalLineObject::boundingRect() const { return QRectF(x_-8, top_, 16, bottom_-top_); }
void VerticalLineObject::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) {
    p->setPen(pen_); p->drawLine(QPointF(x_, top_), QPointF(x_, bottom_));
    if (isSelected()) { p->setPen(QPen(Qt::white,1)); p->setBrush(Qt::white); p->drawEllipse(QPointF(x_, top_), 5,5); p->drawEllipse(QPointF(x_, bottom_), 5,5); }
}
void VerticalLineObject::setXValue(qreal x) { prepareGeometryChange(); x_=x; update(); emit objectModified(); }
int VerticalLineObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, QPointF(x_, top_)).length() < 6) return 0;
    if (QLineF(p, QPointF(x_, bottom_)).length() < 6) return 1;
    return -1;
}
void VerticalLineObject::moveControlPoint(int idx, const QPointF& p) {
    if (idx == 0) { top_ = p.y(); } else if (idx == 1) { bottom_ = p.y(); } x_ = p.x(); emit objectModified();
}
QJsonObject VerticalLineObject::toJson() const { auto o=DrawingObject::toJson(); o["x"]=x_; o["top"]=top_; o["bottom"]=bottom_; return o; }
void VerticalLineObject::fromJson(const QJsonObject& o) { DrawingObject::fromJson(o); x_=o["x"].toDouble(); top_=o["top"].toDouble(); bottom_=o["bottom"].toDouble(); }

// ── FibRetraceObject ────────────────────────────────────────────

constexpr double FibRetraceObject::LEVELS[];

FibRetraceObject::FibRetraceObject(const QPointF& start, const QPointF& end,
                                   const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::FibonacciRetrace, color, 1.2, parent),
      start_(start), end_(end) { updateBounds(); }

void FibRetraceObject::setPoints(const QPointF& start, const QPointF& end) {
    prepareGeometryChange(); start_=start; end_=end; updateBounds(); emit objectModified();
}

void FibRetraceObject::updateBounds() {
    qreal dy = end_.y() - start_.y();
    qreal top = qMin(start_.y(), end_.y());
    qreal bot = qMax(start_.y(), end_.y());
    // Extend for extension levels
    if (dy < 0) bot += qAbs(dy) * 2.618;
    else top -= qAbs(dy) * 2.618;
    bounds_ = QRectF(start_.x() - 20, top - 20, (end_.x() - start_.x()) + 100, bot - top + 40);
}

QRectF FibRetraceObject::boundingRect() const { return bounds_; }

void FibRetraceObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    qreal dy = end_.y() - start_.y();
    for (double level : LEVELS) {
        qreal y = start_.y() + dy * (1.0 - level);
        QColor lc = pen_.color();
        lc.setAlpha(level > 0 && level < 1.0 ? 180 : 120);
        QPen lp(lc, 0.8, Qt::DashLine);
        painter->setPen(lp);
        painter->drawLine(QPointF(start_.x(), y), QPointF(end_.x(), y));
        painter->setFont(textFont_);
        painter->setPen(textColor_);
        painter->drawText(QPointF(end_.x() + 4, y - 2),
                          QString("%1 (%2)").arg(QString::number(level*100, 'f', 1)+"%")
                          .arg(start_.y() + dy * (1.0 - level), 0, 'f', 2));
    }
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(start_, 5,5); painter->drawEllipse(end_, 5,5);
    }
}

int FibRetraceObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, start_).length() < 6) return 0;
    if (QLineF(p, end_).length() < 6) return 1;
    return -1;
}
void FibRetraceObject::moveControlPoint(int idx, const QPointF& p) {
    prepareGeometryChange();
    if (idx == 0) start_=p; else if (idx == 1) end_=p;
    updateBounds(); emit objectModified();
}
QJsonObject FibRetraceObject::toJson() const {
    auto o=DrawingObject::toJson(); o["start"]=QJsonArray({start_.x(),start_.y()});
    o["end"]=QJsonArray({end_.x(),end_.y()}); return o;
}
void FibRetraceObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    auto s=o["start"].toArray(), e=o["end"].toArray();
    if (s.size()==2&&e.size()==2) setPoints(QPointF(s[0].toDouble(),s[1].toDouble()),QPointF(e[0].toDouble(),e[1].toDouble()));
}

// ── GannFanObject ───────────────────────────────────────────────

constexpr double GannFanObject::ANGLES[];

GannFanObject::GannFanObject(const QPointF& origin, qreal scale,
                             const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::GannFan, color, 1.0, parent),
      origin_(origin), scale_(scale) {
    bounds_ = QRectF(origin.x()-scale, origin.y()-scale, scale*2, scale*2);
}

QRectF GannFanObject::boundingRect() const { return bounds_; }

void GannFanObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    qreal len = scale_ * 1.5;
    for (double angle : ANGLES) {
        double rad = qDegreesToRadians(angle);
        qreal ex = origin_.x() + len * qCos(rad);
        qreal ey = origin_.y() - len * qSin(rad);
        QColor lc = pen_.color(); lc.setAlpha(160);
        painter->setPen(QPen(lc, 0.6, Qt::DashLine));
        painter->drawLine(origin_, QPointF(ex, ey));
        painter->setFont(textFont_);
        painter->setPen(textColor_);
        painter->drawText(QPointF(ex+2, ey), QString::number(angle,'f',1)+QChar(0xB0));
    }
    painter->setPen(pen_);
    painter->drawEllipse(origin_, 3, 3);
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(origin_, 5,5);
    }
}

int GannFanObject::controlPointAt(const QPointF& p) const {
    return QLineF(p, origin_).length() < 6 ? 0 : -1;
}
void GannFanObject::moveControlPoint(int idx, const QPointF& p) {
    if (idx == 0) { prepareGeometryChange(); qreal len = QLineF(origin_, p).length();
        if (len > 10) { scale_ = len; } origin_ = p; emit objectModified(); }
}
QJsonObject GannFanObject::toJson() const {
    auto o=DrawingObject::toJson(); o["ox"]=origin_.x(); o["oy"]=origin_.y(); o["scale"]=scale_; return o;
}
void GannFanObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o); origin_=QPointF(o["ox"].toDouble(),o["oy"].toDouble()); scale_=o["scale"].toDouble();
    bounds_ = QRectF(origin_.x()-scale_, origin_.y()-scale_, scale_*2, scale_*2);
}

// ── ElliottWaveObject ───────────────────────────────────────────

ElliottWaveObject::ElliottWaveObject(const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::ElliottWave, color, 1.8, parent) {
    setFlag(ItemIsMovable, true);
}

void ElliottWaveObject::addPoint(const QPointF& pt) { prepareGeometryChange(); points_.append(pt); updateBounds(); emit objectModified(); }
void ElliottWaveObject::setPoints(const QVector<QPointF>& pts) { prepareGeometryChange(); points_=pts; updateBounds(); emit objectModified(); }

void ElliottWaveObject::updateBounds() {
    if (points_.isEmpty()) { bounds_=QRectF(); return; }
    qreal l=points_[0].x(), t=points_[0].y(), r=points_[0].x(), b=points_[0].y();
    for (auto& p : points_) { l=qMin(l,p.x()); t=qMin(t,p.y()); r=qMax(r,p.x()); b=qMax(b,p.y()); }
    bounds_=QRectF(l-20, t-20, r-l+40, b-t+40);
}

QRectF ElliottWaveObject::boundingRect() const { return bounds_; }

void ElliottWaveObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    if (points_.size() < 2) return;
    painter->setPen(pen_);
    for (int i=1; i<points_.size(); ++i)
        painter->drawLine(points_[i-1], points_[i]);
    painter->setFont(QFont("Consolas", 9, QFont::Bold));
    QStringList labels = {"1","2","3","4","5","A","B","C","D","E"};
    for (int i=0; i<points_.size() && i<labels.size(); ++i) {
        QColor lc = (i%2==0) ? QColor(100,255,100) : QColor(255,100,100);
        painter->setPen(lc);
        painter->drawText(points_[i] + QPointF(4, -6), labels[i]);
    }
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        for (auto& p : points_) painter->drawEllipse(p, 4, 4);
    }
}

int ElliottWaveObject::controlPointAt(const QPointF& p) const {
    for (int i=0; i<points_.size(); ++i)
        if (QLineF(p, points_[i]).length() < 7) return i;
    return -1;
}
void ElliottWaveObject::moveControlPoint(int idx, const QPointF& p) {
    if (idx>=0 && idx<points_.size()) { prepareGeometryChange(); points_[idx]=p; updateBounds(); emit objectModified(); }
}
QJsonObject ElliottWaveObject::toJson() const {
    auto o=DrawingObject::toJson(); QJsonArray pts;
    for (auto& p : points_) pts.append(QJsonArray({p.x(),p.y()}));
    o["points"]=pts; return o;
}
void ElliottWaveObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o); QVector<QPointF> pts;
    for (auto v : o["points"].toArray()) { auto a=v.toArray(); pts.append(QPointF(a[0].toDouble(),a[1].toDouble())); }
    setPoints(pts);
}

// ── ChannelObject ───────────────────────────────────────────────

ChannelObject::ChannelObject(const QPointF& p1, const QPointF& p2,
                             const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::Channel, color, 1.2, parent), p1_(p1), p2_(p2) { updateBounds(); }

void ChannelObject::setPoints(const QPointF& p1, const QPointF& p2) { prepareGeometryChange(); p1_=p1; p2_=p2; updateBounds(); emit objectModified(); }
void ChannelObject::updateBounds() {
    qreal l=qMin(p1_.x(),p2_.x())-10,t=qMin(p1_.y(),p2_.y())-10,r=qMax(p1_.x(),p2_.x())+10,b=qMax(p1_.y(),p2_.y())+10;
    // Add offset for parallel line
    qreal dy = p2_.y() - p1_.y(), dx = p2_.x() - p1_.x();
    qreal len = qSqrt(dx*dx+dy*dy);
    if (len > 0) {
        qreal ox = -dy/len * 30, oy = dx/len * 30;
        l = qMin(l, qMin(p1_.x()+ox, p2_.x()+ox));
        t = qMin(t, qMin(p1_.y()+oy, p2_.y()+oy));
        r = qMax(r, qMax(p1_.x()+ox, p2_.x()+ox));
        b = qMax(b, qMax(p1_.y()+oy, p2_.y()+oy));
    }
    bounds_=QRectF(l,t,r-l,b-t);
}

QRectF ChannelObject::boundingRect() const { return bounds_; }

void ChannelObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen_);
    painter->drawLine(p1_, p2_);
    qreal dy=p2_.y()-p1_.y(), dx=p2_.x()-p1_.x();
    qreal len=qSqrt(dx*dx+dy*dy);
    if (len > 0) {
        qreal ox=-dy/len*30, oy=dx/len*30;
        painter->setPen(QPen(pen_.color(), pen_.widthF(), Qt::DashLine));
        painter->drawLine(p1_+QPointF(ox,oy), p2_+QPointF(ox,oy));
    }
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(p1_,5,5); painter->drawEllipse(p2_,5,5);
    }
}
int ChannelObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p,p1_).length()<6) return 0;
    if (QLineF(p,p2_).length()<6) return 1;
    return -1;
}
void ChannelObject::moveControlPoint(int idx, const QPointF& p) { prepareGeometryChange(); if(idx==0)p1_=p; else if(idx==1)p2_=p; updateBounds(); emit objectModified(); }
QJsonObject ChannelObject::toJson() const {
    auto o=DrawingObject::toJson(); o["p1"]=QJsonArray({p1_.x(),p1_.y()}); o["p2"]=QJsonArray({p2_.x(),p2_.y()}); return o;
}
void ChannelObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o); auto a1=o["p1"].toArray(), a2=o["p2"].toArray();
    if (a1.size()==2&&a2.size()==2) setPoints(QPointF(a1[0].toDouble(),a1[1].toDouble()),QPointF(a2[0].toDouble(),a2[1].toDouble()));
}

// ── TextLabel ────────────────────────────────────────────────────

TextLabelObject::TextLabelObject(const QPointF& pos, const QString& text,
                                  const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::TextLabel, color, 1.0, parent), pos_(pos), text_(text) {
    setFlag(ItemIsMovable, true);
    updateBounds();
}

void TextLabelObject::updateBounds() {
    QFontMetricsF fm(textFont_);
    qreal w = fm.horizontalAdvance(text_) + MARGIN * 2;
    qreal h = fm.height() * 2 + MARGIN * 2;
    bounds_ = QRectF(pos_.x() - MARGIN, pos_.y() - h - MARGIN, w, h);
}

QRectF TextLabelObject::boundingRect() const {
    return bounds_.adjusted(-2, -2, 2, 2);
}

void TextLabelObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing);
    QFontMetricsF fm(textFont_);

    // Background
    painter->setBrush(QColor(20, 20, 30, 200));
    painter->setPen(QPen(pen_.color(), 1));
    QRectF bg = bounds_.adjusted(0, 0, 0, 0);
    painter->drawRoundedRect(bg, 3, 3);

    // Text
    painter->setPen(pen_.color());
    painter->setFont(textFont_);
    painter->drawText(bg.adjusted(MARGIN, MARGIN, -MARGIN, -MARGIN),
                      Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text_);

    if (isSelected()) {
        painter->setPen(QPen(Qt::white, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(bg);
    }
}

int TextLabelObject::controlPointAt(const QPointF& scenePos) const {
    return boundingRect().contains(scenePos) ? 0 : -1;
}

void TextLabelObject::moveControlPoint(int index, const QPointF& newPos) {
    if (index == 0) { setPos(newPos); updateBounds(); emit objectModified(); }
}

QJsonObject TextLabelObject::toJson() const {
    auto o = DrawingObject::toJson();
    o["pos"] = QJsonArray({pos_.x(), pos_.y()});
    o["text"] = text_;
    return o;
}

void TextLabelObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    auto a = o["pos"].toArray();
    if (a.size() == 2) pos_ = QPointF(a[0].toDouble(), a[1].toDouble());
    text_ = o["text"].toString();
    updateBounds();
}

// ── RayObject ────────────────────────────────────────────────────

RayObject::RayObject(const QPointF& origin, const QPointF& direction,
                     const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::Ray, color, 1.5, parent), origin_(origin), direction_(direction) {
    updateBounds();
}

void RayObject::updateBounds() {
    qreal dx = direction_.x() - origin_.x();
    qreal dy = direction_.y() - origin_.y();
    qreal len = qSqrt(dx*dx + dy*dy);
    qreal extent = 10000.0;
    if (len > 0) {
        qreal ex = origin_.x() + dx / len * extent;
        qreal ey = origin_.y() + dy / len * extent;
        qreal l = qMin(origin_.x(), ex) - 10;
        qreal t = qMin(origin_.y(), ey) - 10;
        qreal r = qMax(origin_.x(), ex) + 10;
        qreal b = qMax(origin_.y(), ey) + 10;
        bounds_ = QRectF(l, t, r-l, b-t);
    } else {
        bounds_ = QRectF(origin_.x()-10, origin_.y()-10, 20, 20);
    }
}

QRectF RayObject::boundingRect() const { return bounds_; }

void RayObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen_);
    qreal dx = direction_.x() - origin_.x();
    qreal dy = direction_.y() - origin_.y();
    qreal len = qSqrt(dx*dx + dy*dy);
    if (len > 0) {
        qreal extent = 10000.0;
        QPointF farPt(origin_.x() + dx/len*extent, origin_.y() + dy/len*extent);
        painter->drawLine(origin_, farPt);
    }
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(origin_, 5,5); painter->drawEllipse(direction_, 5,5);
    }
}

int RayObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, origin_).length() < 6) return 0;
    if (QLineF(p, direction_).length() < 6) return 1;
    return -1;
}

void RayObject::moveControlPoint(int idx, const QPointF& p) {
    prepareGeometryChange();
    if (idx == 0) origin_ = p;
    else if (idx == 1) direction_ = p;
    updateBounds(); emit objectModified();
}

QJsonObject RayObject::toJson() const {
    auto o=DrawingObject::toJson();
    o["origin"]=QJsonArray({origin_.x(),origin_.y()});
    o["direction"]=QJsonArray({direction_.x(),direction_.y()});
    return o;
}

void RayObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    auto a1=o["origin"].toArray(), a2=o["direction"].toArray();
    if (a1.size()==2&&a2.size()==2) {
        origin_=QPointF(a1[0].toDouble(),a1[1].toDouble());
        direction_=QPointF(a2[0].toDouble(),a2[1].toDouble());
        updateBounds();
    }
}

// ── FibArcObject ─────────────────────────────────────────────────

constexpr double FibArcObject::LEVELS[];

FibArcObject::FibArcObject(const QPointF& start, const QPointF& end,
                           const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::FibonacciArc, color, 1.2, parent),
      start_(start), end_(end) { updateBounds(); }

void FibArcObject::setPoints(const QPointF& start, const QPointF& end) {
    prepareGeometryChange(); start_=start; end_=end; updateBounds(); emit objectModified();
}

void FibArcObject::updateBounds() {
    qreal dy = end_.y() - start_.y();
    qreal top = qMin(start_.y(), end_.y());
    qreal bot = qMax(start_.y(), end_.y());
    qreal maxArc = qAbs(dy) * 0.618 * 0.5;
    if (dy > 0) bot += maxArc;
    else top -= maxArc;
    bounds_ = QRectF(start_.x() - 20, top - 20, (end_.x() - start_.x()) + 40, bot - top + 40);
}

QRectF FibArcObject::boundingRect() const { return bounds_; }

void FibArcObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    qreal dy = end_.y() - start_.y();
    qreal midX = (start_.x() + end_.x()) * 0.5;
    for (double level : LEVELS) {
        qreal y = start_.y() + dy * (1.0 - level);
        qreal arcHeight = dy * level * 0.4;
        QPainterPath path;
        path.moveTo(start_.x(), y);
        path.cubicTo(midX, y + arcHeight, midX, y + arcHeight, end_.x(), y);
        QColor lc = pen_.color(); lc.setAlpha(180);
        painter->setPen(QPen(lc, 0.8));
        painter->drawPath(path);
        painter->setFont(textFont_);
        painter->setPen(textColor_);
        painter->drawText(QPointF(midX - 16, y + arcHeight - 6),
                          QString("%1").arg(level*100, 0, 'f', 1) + "%");
    }
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(start_, 5,5); painter->drawEllipse(end_, 5,5);
    }
}

int FibArcObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, start_).length() < 6) return 0;
    if (QLineF(p, end_).length() < 6) return 1;
    return -1;
}

void FibArcObject::moveControlPoint(int idx, const QPointF& p) {
    prepareGeometryChange();
    if (idx == 0) start_=p; else if (idx == 1) end_=p;
    updateBounds(); emit objectModified();
}

QJsonObject FibArcObject::toJson() const {
    auto o=DrawingObject::toJson(); o["start"]=QJsonArray({start_.x(),start_.y()});
    o["end"]=QJsonArray({end_.x(),end_.y()}); return o;
}

void FibArcObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    auto s=o["start"].toArray(), e=o["end"].toArray();
    if (s.size()==2&&e.size()==2) setPoints(QPointF(s[0].toDouble(),s[1].toDouble()),QPointF(e[0].toDouble(),e[1].toDouble()));
}

// ── GannSquareObject ─────────────────────────────────────────────

GannSquareObject::GannSquareObject(const QPointF& origin, qreal scale,
                                   const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::GannSquare, color, 1.0, parent),
      origin_(origin), scale_(scale) {
    bounds_ = QRectF(origin_.x()-scale_, origin_.y()-scale_, scale_*2, scale_*2);
}

QRectF GannSquareObject::boundingRect() const { return bounds_; }

void GannSquareObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    qreal left = origin_.x() - scale_;
    qreal top = origin_.y() - scale_;
    qreal right = origin_.x() + scale_;
    qreal bot = origin_.y() + scale_;

    painter->setPen(QPen(pen_.color(), pen_.widthF()));
    painter->drawRect(QRectF(left, top, scale_*2, scale_*2));

    QPen gridPen(pen_.color(), 0.6, Qt::DashLine);
    gridPen.setCosmetic(true);
    painter->setPen(gridPen);

    for (int i = 1; i < 8; ++i) {
        qreal frac = i / 8.0;
        qreal x = left + scale_ * 2 * frac;
        qreal y = top + scale_ * 2 * frac;
        painter->drawLine(QPointF(x, top), QPointF(x, bot));
        painter->drawLine(QPointF(left, y), QPointF(right, y));
    }

    QPen diagPen(pen_.color(), 0.8, Qt::DotLine);
    diagPen.setCosmetic(true);
    painter->setPen(diagPen);

    painter->drawLine(QPointF(left, top), QPointF(right, bot));
    painter->drawLine(QPointF(left, bot), QPointF(right, top));
    painter->drawLine(QPointF(origin_.x(), top), QPointF(origin_.x(), bot));
    painter->drawLine(QPointF(left, origin_.y()), QPointF(right, origin_.y()));

    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(origin_, 5,5);
    }
}

int GannSquareObject::controlPointAt(const QPointF& p) const {
    return QLineF(p, origin_).length() < 6 ? 0 : -1;
}

void GannSquareObject::moveControlPoint(int idx, const QPointF& p) {
    if (idx == 0) {
        prepareGeometryChange();
        qreal len = QLineF(origin_, p).length();
        if (len > 10) { scale_ = len; }
        origin_ = p;
        bounds_ = QRectF(origin_.x()-scale_, origin_.y()-scale_, scale_*2, scale_*2);
        emit objectModified();
    }
}

QJsonObject GannSquareObject::toJson() const {
    auto o=DrawingObject::toJson(); o["ox"]=origin_.x(); o["oy"]=origin_.y(); o["scale"]=scale_; return o;
}

void GannSquareObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o); origin_=QPointF(o["ox"].toDouble(),o["oy"].toDouble()); scale_=o["scale"].toDouble();
    bounds_ = QRectF(origin_.x()-scale_, origin_.y()-scale_, scale_*2, scale_*2);
}

// ── AndrewsPitchforkObject ───────────────────────────────────────

AndrewsPitchforkObject::AndrewsPitchforkObject(const QPointF& left, const QPointF& right,
                                               const QPointF& apex,
                                               const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::AndrewsPitchfork, color, 1.2, parent),
      left_(left), right_(right), apex_(apex) { updateBounds(); }

void AndrewsPitchforkObject::setPoints(const QPointF& left, const QPointF& right, const QPointF& apex) {
    prepareGeometryChange(); left_=left; right_=right; apex_=apex; updateBounds(); emit objectModified();
}

void AndrewsPitchforkObject::updateBounds() {
    qreal extent = 10000.0;
    QPointF mid = (left_ + right_) * 0.5;
    qreal dx = mid.x() - apex_.x();
    qreal dy = mid.y() - apex_.y();
    qreal len = qSqrt(dx*dx+dy*dy);
    qreal l=0, t=0, r=0, b=0;
    auto extendBounds = [&](const QPointF& pt) {
        l = qMin(l, pt.x()); t = qMin(t, pt.y());
        r = qMax(r, pt.x()); b = qMax(b, pt.y());
    };
    if (len > 0) {
        dx /= len; dy /= len;
        QPointF dir(dx * extent, dy * extent);
        extendBounds(apex_); extendBounds(apex_ + dir);
        extendBounds(left_); extendBounds(left_ + dir);
        extendBounds(right_); extendBounds(right_ + dir);
        extendBounds(mid); extendBounds(mid + dir);
    } else {
        extendBounds(apex_); extendBounds(left_); extendBounds(right_);
    }
    bounds_ = QRectF(l-10, t-10, r-l+20, b-t+20);
}

QRectF AndrewsPitchforkObject::boundingRect() const { return bounds_; }

void AndrewsPitchforkObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    QPointF mid = (left_ + right_) * 0.5;
    qreal dx = mid.x() - apex_.x();
    qreal dy = mid.y() - apex_.y();
    qreal len = qSqrt(dx*dx+dy*dy);
    if (len > 0) {
        qreal extent = 10000.0;
        qreal nx = dx / len, ny = dy / len;
        QPointF dir(nx * extent, ny * extent);
        painter->setPen(pen_);
        painter->drawLine(apex_, apex_ + dir);
        painter->setPen(QPen(pen_.color(), pen_.widthF(), Qt::DashLine));
        painter->drawLine(left_, left_ + dir);
        painter->drawLine(right_, right_ + dir);
        painter->setPen(QPen(pen_.color(), 0.6, Qt::DotLine));
        painter->drawLine(apex_, mid);
    }
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(left_, 5,5); painter->drawEllipse(right_, 5,5);
        painter->drawEllipse(apex_, 5,5);
    }
}

int AndrewsPitchforkObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, left_).length() < 6) return 0;
    if (QLineF(p, right_).length() < 6) return 1;
    if (QLineF(p, apex_).length() < 6) return 2;
    return -1;
}

void AndrewsPitchforkObject::moveControlPoint(int idx, const QPointF& p) {
    prepareGeometryChange();
    if (idx == 0) left_ = p;
    else if (idx == 1) right_ = p;
    else if (idx == 2) apex_ = p;
    updateBounds(); emit objectModified();
}

QJsonObject AndrewsPitchforkObject::toJson() const {
    auto o=DrawingObject::toJson();
    o["left"]=QJsonArray({left_.x(),left_.y()});
    o["right"]=QJsonArray({right_.x(),right_.y()});
    o["apex"]=QJsonArray({apex_.x(),apex_.y()});
    return o;
}

void AndrewsPitchforkObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    auto l=o["left"].toArray(), r=o["right"].toArray(), a=o["apex"].toArray();
    if (l.size()==2&&r.size()==2&&a.size()==2)
        setPoints(QPointF(l[0].toDouble(),l[1].toDouble()),
                  QPointF(r[0].toDouble(),r[1].toDouble()),
                  QPointF(a[0].toDouble(),a[1].toDouble()));
}

// ── CycleLineObject ──────────────────────────────────────────────

CycleLineObject::CycleLineObject(const QPointF& start, const QPointF& intervalPoint,
                                 const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::CycleLine, color, 1.0, parent),
      start_(start), interval_(qAbs(intervalPoint.x() - start.x())) {
    if (interval_ < 1.0) interval_ = 20.0;
    updateBounds();
}

void CycleLineObject::updateBounds() {
    qreal cycles = 20.0;
    qreal left = start_.x() - interval_ * cycles;
    qreal right = start_.x() + interval_ * cycles;
    qreal top = -10000.0;
    qreal bot = 10000.0;
    bounds_ = QRectF(left, top, right-left, bot-top);
}

QRectF CycleLineObject::boundingRect() const { return bounds_; }

void CycleLineObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    QPen cyclePen(pen_.color(), 0.8, Qt::DashLine);
    painter->setPen(cyclePen);
    qreal cycles = 20.0;
    qreal top = start_.y() - 5000;
    qreal bot = start_.y() + 5000;
    for (qreal i = -cycles; i <= cycles; i += 1.0) {
        qreal x = start_.x() + i * interval_;
        painter->drawLine(QPointF(x, top), QPointF(x, bot));
    }
    painter->setPen(pen_);
    painter->drawLine(QPointF(start_.x(), top), QPointF(start_.x(), bot));
    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(start_, 5,5);
        painter->drawEllipse(QPointF(start_.x()+interval_, start_.y()), 5,5);
    }
}

int CycleLineObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, start_).length() < 6) return 0;
    if (QLineF(p, QPointF(start_.x()+interval_, start_.y())).length() < 6) return 1;
    return -1;
}

void CycleLineObject::moveControlPoint(int idx, const QPointF& p) {
    prepareGeometryChange();
    if (idx == 0) { start_ = p; }
    else if (idx == 1) { interval_ = qAbs(p.x() - start_.x()); if (interval_ < 1.0) interval_ = 1.0; }
    updateBounds(); emit objectModified();
}

QJsonObject CycleLineObject::toJson() const {
    auto o=DrawingObject::toJson();
    o["sx"]=start_.x(); o["sy"]=start_.y(); o["interval"]=interval_;
    return o;
}

void CycleLineObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    start_ = QPointF(o["sx"].toDouble(), o["sy"].toDouble());
    interval_ = o["interval"].toDouble();
    if (interval_ < 1.0) interval_ = 20.0;
    updateBounds();
}

// ── BrushObject ──────────────────────────────────────────────────

BrushObject::BrushObject(const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::Brush, color, 2.0, parent) {}

void BrushObject::addPoint(const QPointF& pt) {
    prepareGeometryChange(); points_.append(pt); updateBounds(); emit objectModified();
}

void BrushObject::setPoints(const QVector<QPointF>& pts) {
    prepareGeometryChange(); points_=pts; updateBounds(); emit objectModified();
}

void BrushObject::updateBounds() {
    if (points_.isEmpty()) { bounds_=QRectF(); return; }
    qreal l=points_[0].x(), t=points_[0].y(), r=points_[0].x(), b=points_[0].y();
    for (auto& p : points_) {
        l=qMin(l,p.x()); t=qMin(t,p.y()); r=qMax(r,p.x()); b=qMax(b,p.y());
    }
    bounds_=QRectF(l-20, t-20, r-l+40, b-t+40);
}

QRectF BrushObject::boundingRect() const { return bounds_; }

void BrushObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    if (points_.size() < 2) return;
    painter->setPen(pen_);
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.moveTo(points_[0]);
    for (int i=1; i<points_.size(); ++i)
        path.lineTo(points_[i]);
    painter->drawPath(path);
    if (isSelected() && points_.size() > 0) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        int step = qMax(1, points_.size() / 20);
        for (int i=0; i<points_.size(); i+=step)
            painter->drawEllipse(points_[i], 3, 3);
    }
}

int BrushObject::controlPointAt(const QPointF& p) const {
    int step = qMax(1, points_.size() / 20);
    for (int i=0; i<points_.size(); i+=step)
        if (QLineF(p, points_[i]).length() < 7) return i;
    return -1;
}

void BrushObject::moveControlPoint(int idx, const QPointF& p) {
    if (idx>=0 && idx<points_.size()) {
        prepareGeometryChange(); points_[idx]=p; updateBounds(); emit objectModified();
    }
}

QJsonObject BrushObject::toJson() const {
    auto o=DrawingObject::toJson();
    QJsonArray pts;
    for (auto& p : points_) pts.append(QJsonArray({p.x(),p.y()}));
    o["points"]=pts; return o;
}

void BrushObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    QVector<QPointF> pts;
    for (auto v : o["points"].toArray()) {
        auto a=v.toArray(); pts.append(QPointF(a[0].toDouble(),a[1].toDouble()));
    }
    setPoints(pts);
}

// ── MeasureObject ────────────────────────────────────────────────

MeasureObject::MeasureObject(const QPointF& p1, const QPointF& p2,
                             const QColor& color, QGraphicsItem* parent)
    : DrawingObject(DrawingToolType::Measure, color, 1.2, parent), p1_(p1), p2_(p2) {
    updateBounds();
}

void MeasureObject::updateBounds() {
    qreal l=qMin(p1_.x(),p2_.x())-80, t=qMin(p1_.y(),p2_.y())-40;
    qreal r=qMax(p1_.x(),p2_.x())+80, b=qMax(p1_.y(),p2_.y())+40;
    bounds_=QRectF(l,t,r-l,b-t);
}

QRectF MeasureObject::boundingRect() const { return bounds_; }

void MeasureObject::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(pen_);
    painter->drawLine(p1_, p2_);

    qreal dx = p2_.x() - p1_.x();
    qreal dy = p2_.y() - p1_.y();
    qreal dist = qSqrt(dx*dx + dy*dy);
    qreal angle = (dist > 0) ? qRadiansToDegrees(qAtan2(-dy, dx)) : 0;
    qreal pct = (p1_.y() != 0) ? qAbs(dy / p1_.y() * 100.0) : 0;

    QString label = QString("H:%1 V:%2 Angle:%3%4 Chg:%5%")
                    .arg(qAbs(dx), 0, 'f', 1)
                    .arg(qAbs(dy), 0, 'f', 1)
                    .arg(angle, 0, 'f', 1).arg(QChar(0xB0))
                    .arg(pct, 0, 'f', 2);

    QPointF mid = (p1_ + p2_) * 0.5;
    painter->setFont(textFont_);
    QFontMetricsF fm(textFont_);
    qreal tw = fm.horizontalAdvance(label);
    qreal th = fm.height() * 2;

    QRectF bg(mid.x() - tw*0.5 - 4, mid.y() - th - 4, tw + 8, th + 8);
    painter->setBrush(QColor(20,20,30,200));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(bg, 3, 3);
    painter->setPen(textColor_);
    painter->drawText(bg.adjusted(4, 2, -4, -2), Qt::AlignLeft | Qt::AlignTop, label);

    if (isSelected()) {
        painter->setPen(QPen(Qt::white,1)); painter->setBrush(Qt::white);
        painter->drawEllipse(p1_, 5,5); painter->drawEllipse(p2_, 5,5);
    }
}

int MeasureObject::controlPointAt(const QPointF& p) const {
    if (QLineF(p, p1_).length() < 6) return 0;
    if (QLineF(p, p2_).length() < 6) return 1;
    return -1;
}

void MeasureObject::moveControlPoint(int idx, const QPointF& p) {
    prepareGeometryChange();
    if (idx == 0) p1_ = p; else if (idx == 1) p2_ = p;
    updateBounds(); emit objectModified();
}

QJsonObject MeasureObject::toJson() const {
    auto o=DrawingObject::toJson();
    o["p1"]=QJsonArray({p1_.x(),p1_.y()});
    o["p2"]=QJsonArray({p2_.x(),p2_.y()});
    return o;
}

void MeasureObject::fromJson(const QJsonObject& o) {
    DrawingObject::fromJson(o);
    auto a1=o["p1"].toArray(), a2=o["p2"].toArray();
    if (a1.size()==2&&a2.size()==2) {
        p1_=QPointF(a1[0].toDouble(),a1[1].toDouble());
        p2_=QPointF(a2[0].toDouble(),a2[1].toDouble());
        updateBounds();
    }
}

} // namespace fincept::screens
