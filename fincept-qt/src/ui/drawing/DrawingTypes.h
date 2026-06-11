#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QUndoCommand>

namespace fincept::ui {

enum class DrawingToolType {
    None,
    TrendLine,
    HorizontalLine,
    VerticalLine,
    Channel,
    Ray,
    ExtendedLine,
    FibonacciRetrace,
    FibonacciExtension,
    FibonacciFan,
    FibonacciArc,
    FibonacciTimeZone,
    GannFan,
    GannSquare,
    ElliottWave,
    AndrewsPitchfork,
    CycleLine,
    TextLabel,
    Brush,
    Measure,
    Rectangle,
    Circle,
    Triangle,
    Arrow,
    ArrowMarker,
    PriceLabel,
    DateLabel,
    SignalLabel,
    CrossLine,
    RiskReward,
    HeadAndShoulders,
    ABCD,
    Cypher,
    Wedge,
    ParallelChannel
};

struct DrawingPoint {
    double price = 0;
    qint64 time_ms = 0;

    QPointF to_point() const { return QPointF(static_cast<qreal>(time_ms), price); }

    static DrawingPoint from_point(const QPointF& pt) {
        return {pt.y(), static_cast<qint64>(pt.x())};
    }
};

struct DrawingProperties {
    QColor line_color{Qt::white};
    double line_width = 1.5;
    Qt::PenStyle line_style = Qt::SolidLine;
    QColor fill_color{QColor(255, 255, 255, 30)};
    bool fill_enabled = false;
    QString text;
    int font_size = 12;
};

struct DrawingObject {
    QString id;
    DrawingToolType tool_type = DrawingToolType::None;
    QVector<DrawingPoint> points;
    DrawingProperties props;
    bool visible = true;
    qint64 created_at = 0;
    qint64 modified_at = 0;
};

class DrawingUndoCommand : public QUndoCommand {
public:
    enum Action { Add, Remove, Modify, MovePoint };

    DrawingUndoCommand(Action action, const DrawingObject& before,
                       const DrawingObject& after, int point_index = -1);

    void undo() override;
    void redo() override;

private:
    Action action_;
    DrawingObject before_;
    DrawingObject after_;
    int point_index_;
};

} // namespace fincept::ui
