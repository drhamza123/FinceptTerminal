#pragma once
#include <QObject>
#include <QVector>
#include <QPointF>

class QTimer;

namespace fincept::screens {

class BarReplayEngine : public QObject {
    Q_OBJECT
public:
    explicit BarReplayEngine(QObject* parent = nullptr);
    void loadData(const QVector<QPointF>& data);
    void play();
    void pause();
    void stepForward();
    void stepBackward();
    void stop();
    void setSpeed(int barsPerSecond);
    int currentIndex() const;
    int totalBars() const;
    bool isPlaying() const;
    QVector<QPointF> visibleData() const;

signals:
    void barChanged(int index);
    void replayFinished();

private:
    void tick();
    QVector<QPointF> data_;
    int currentIndex_ = 0;
    int speed_ = 5;
    bool playing_ = false;
    QTimer* timer_ = nullptr;
};

} // namespace fincept::screens
