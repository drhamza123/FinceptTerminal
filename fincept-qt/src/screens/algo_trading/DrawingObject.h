// DrawingObject.h — Interactive analytical drawing tools for MT5 Fleet Chart
#pragma once
#include <QGraphicsObject>
#include <QJsonObject>
#include <QCursor>
#include <QPen>
#include <QFont>
#include <QVector>

namespace fincept::screens {

// ── Drawing Tool Types ──────────────────────────────────────────

enum class DrawingToolType {
    TrendLine, HorizontalLine, VerticalLine, Channel, Ray,
    FibonacciRetrace, FibonacciExtension, FibonacciFan,
    FibonacciArc, GannFan, GannSquare, ElliottWave,
    AndrewsPitchfork, CycleLine, Crosshair, Brush, Measure, TextLabel, None
};

// ── DrawingObject Base Class ────────────────────────────────────

class DrawingObject : public QGraphicsObject {
    Q_OBJECT
  public:
    enum { Type = QGraphicsItem::UserType + 100 };
    int type() const override { return Type; }

    DrawingObject(DrawingToolType toolType, const QColor& color = QColor(100,180,255),
                  qreal width = 1.5, QGraphicsItem* parent = nullptr);
    ~DrawingObject() override;

    DrawingToolType toolType() const { return toolType_; }
    QString toolName() const;
    QString id() const { return id_; }
    void setId(const QString& id) { id_ = id; }

    QColor color() const { return pen_.color(); }
    void setColor(const QColor& c);
    qreal lineWidth() const { return pen_.widthF(); }
    void setLineWidth(qreal w);

    bool isLocked() const { return locked_; }
    void setLocked(bool locked);

    bool isVisible() const { return visible_; }
    void setVisible(bool v);

    // Serialization
    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& obj);

    // Hit test for control points
    virtual int controlPointAt(const QPointF& scenePos) const { return -1; }
    virtual void moveControlPoint(int index, const QPointF& newPos) {}

  signals:
    void objectModified();
    void objectSelected(DrawingObject* obj);
    void objectDeleted(DrawingObject* obj);

  protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    QPen pen_;
    DrawingToolType toolType_;
    QString id_;
    bool locked_ = false;
    bool visible_ = true;
    bool dragging_ = false;
    QPointF dragStart_;
    QPointF dragOrigin_;
    QColor textColor_ = QColor(200,200,200);
    QFont textFont_ = QFont("Consolas", 8);
};

// ── TrendLine ──────────────────────────────────────────────────

class TrendLineObject : public DrawingObject {
    Q_OBJECT
  public:
    TrendLineObject(const QPointF& start = {}, const QPointF& end = {},
                    const QColor& color = QColor(100,180,255), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    QPointF startPoint() const { return start_; }
    QPointF endPoint() const { return end_; }
    void setPoints(const QPointF& start, const QPointF& end);

    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

  private:
    QPointF start_, end_;
    void updateBounds();
    QRectF bounds_;
    static constexpr qreal CP_RADIUS = 5.0;
};

// ── HorizontalLine ──────────────────────────────────────────────

class HorizontalLineObject : public DrawingObject {
    Q_OBJECT
  public:
    HorizontalLineObject(qreal y = 0, qreal left = 0, qreal right = 1000,
                         const QColor& color = QColor(255,200,100), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    qreal yValue() const { return y_; }
    void setYValue(qreal y);

    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

  private:
    qreal y_, left_, right_;
};

// ── VerticalLine ────────────────────────────────────────────────

class VerticalLineObject : public DrawingObject {
    Q_OBJECT
  public:
    VerticalLineObject(qreal x = 0, qreal top = 0, qreal bottom = 1000,
                       const QColor& color = QColor(100,200,255), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    qreal xValue() const { return x_; }
    void setXValue(qreal x);

    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

  private:
    qreal x_, top_, bottom_;
};

// ── Fibonacci Retrace ──────────────────────────────────────────

class FibRetraceObject : public DrawingObject {
    Q_OBJECT
  public:
    FibRetraceObject(const QPointF& start = {}, const QPointF& end = {},
                     const QColor& color = QColor(100,180,255), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    void setPoints(const QPointF& start, const QPointF& end);
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

    static constexpr double LEVELS[] = {0.0, 0.236, 0.382, 0.5, 0.618, 0.786, 1.0, 1.272, 1.414, 1.618, 2.0, 2.618, 3.618};

  private:
    QPointF start_, end_;
    QRectF bounds_;
    void updateBounds();
};

// ── Gann Fan ────────────────────────────────────────────────────

class GannFanObject : public DrawingObject {
    Q_OBJECT
  public:
    GannFanObject(const QPointF& origin = {}, qreal scale = 100.0,
                  const QColor& color = QColor(255,200,100), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

  private:
    QPointF origin_;
    qreal scale_;
    QRectF bounds_;
    static constexpr double ANGLES[] = {82.5, 75, 71.25, 63.75, 45, 26.25, 18.75, 15, 7.5};
};

// ── Elliott Wave ────────────────────────────────────────────────

class ElliottWaveObject : public DrawingObject {
    Q_OBJECT
  public:
    ElliottWaveObject(const QColor& color = QColor(255,150,100), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    void addPoint(const QPointF& pt);
    void setPoints(const QVector<QPointF>& pts);
    const QVector<QPointF>& points() const { return points_; }

    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

  private:
    QVector<QPointF> points_;
    QRectF bounds_;
    void updateBounds();
};

// ── TextLabel ────────────────────────────────────────────────────

class TextLabelObject : public DrawingObject {
    Q_OBJECT
  public:
    TextLabelObject(const QPointF& pos = {}, const QString& text = "",
                    const QColor& color = QColor(200,200,200), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    void setText(const QString& t) { text_ = t; prepareGeometryChange(); }
    QString text() const { return text_; }
    QPointF position() const { return pos_; }
    void setPosition(const QPointF& p) { pos_ = p; prepareGeometryChange(); }

    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

  private:
    QPointF pos_;
    QString text_;
    QRectF bounds_;
    void updateBounds();
    static constexpr qreal MARGIN = 6.0;
};

// ── Channel ─────────────────────────────────────────────────────

class ChannelObject : public DrawingObject {
    Q_OBJECT
  public:
    ChannelObject(const QPointF& p1 = {}, const QPointF& p2 = {},
                  const QColor& color = QColor(100,255,150), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    void setPoints(const QPointF& p1, const QPointF& p2);
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

  private:
    QPointF p1_, p2_;
    QRectF bounds_;
    void updateBounds();
};

// ── Ray ──────────────────────────────────────────────────────────

class RayObject : public DrawingObject {
    Q_OBJECT
  public:
    RayObject(const QPointF& origin = {}, const QPointF& direction = {},
              const QColor& color = QColor(100,180,255), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;
  private:
    QPointF origin_, direction_;
    QRectF bounds_;
    void updateBounds();
};

// ── Fib Arc ──────────────────────────────────────────────────────

class FibArcObject : public DrawingObject {
    Q_OBJECT
  public:
    FibArcObject(const QPointF& start = {}, const QPointF& end = {},
                 const QColor& color = QColor(100,180,255), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    void setPoints(const QPointF& start, const QPointF& end);
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;
    static constexpr double LEVELS[] = {0.382, 0.5, 0.618};
  private:
    QPointF start_, end_;
    QRectF bounds_;
    void updateBounds();
};

// ── Gann Square ─────────────────────────────────────────────────

class GannSquareObject : public DrawingObject {
    Q_OBJECT
  public:
    GannSquareObject(const QPointF& origin = {}, qreal scale = 100.0,
                     const QColor& color = QColor(255,200,100), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;
  private:
    QPointF origin_;
    qreal scale_;
    QRectF bounds_;
};

// ── Pitchfork ────────────────────────────────────────────────────

class AndrewsPitchforkObject : public DrawingObject {
    Q_OBJECT
  public:
    AndrewsPitchforkObject(const QPointF& left = {}, const QPointF& right = {},
                           const QPointF& apex = {},
                           const QColor& color = QColor(100,255,150), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    void setPoints(const QPointF& left, const QPointF& right, const QPointF& apex);
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;
  private:
    QPointF left_, right_, apex_;
    QRectF bounds_;
    void updateBounds();
};

// ── Cycle Line ───────────────────────────────────────────────────

class CycleLineObject : public DrawingObject {
    Q_OBJECT
  public:
    CycleLineObject(const QPointF& start = {}, const QPointF& intervalPoint = {},
                    const QColor& color = QColor(100,200,255), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;
  private:
    QPointF start_;
    qreal interval_;
    QRectF bounds_;
    void updateBounds();
};

// ── Brush ────────────────────────────────────────────────────────

class BrushObject : public DrawingObject {
    Q_OBJECT
  public:
    BrushObject(const QColor& color = QColor(100,180,255), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    void addPoint(const QPointF& pt);
    void setPoints(const QVector<QPointF>& pts);
    const QVector<QPointF>& points() const { return points_; }
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;
  private:
    QVector<QPointF> points_;
    QRectF bounds_;
    void updateBounds();
};

// ── Measure ──────────────────────────────────────────────────────

class MeasureObject : public DrawingObject {
    Q_OBJECT
  public:
    MeasureObject(const QPointF& p1 = {}, const QPointF& p2 = {},
                  const QColor& color = QColor(255,200,100), QGraphicsItem* parent = nullptr);
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;
    int controlPointAt(const QPointF& scenePos) const override;
    void moveControlPoint(int index, const QPointF& newPos) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;
  private:
    QPointF p1_, p2_;
    QRectF bounds_;
    void updateBounds();
};

} // namespace fincept::screens
