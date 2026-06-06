#include "screens/algo_trading/BarReplayEngine.h"
#include <QTimer>
#include <algorithm>

namespace fincept::screens {

BarReplayEngine::BarReplayEngine(QObject* parent)
    : QObject(parent)
    , timer_(new QTimer(this))
{
    timer_->setSingleShot(false);
    connect(timer_, &QTimer::timeout, this, &BarReplayEngine::tick);
}

void BarReplayEngine::loadData(const QVector<QPointF>& data)
{
    stop();
    data_ = data;
    currentIndex_ = 0;
    emit barChanged(currentIndex_);
}

void BarReplayEngine::play()
{
    if (data_.isEmpty() || playing_) return;
    if (currentIndex_ >= data_.size() - 1) {
        currentIndex_ = 0;
    }
    playing_ = true;
    int interval = std::max(50, 1000 / speed_);
    timer_->setInterval(interval);
    timer_->start();
}

void BarReplayEngine::pause()
{
    if (!playing_) return;
    playing_ = false;
    timer_->stop();
}

void BarReplayEngine::stepForward()
{
    if (data_.isEmpty()) return;
    if (currentIndex_ < data_.size() - 1) {
        currentIndex_++;
        emit barChanged(currentIndex_);
    }
    if (currentIndex_ >= data_.size() - 1) {
        emit replayFinished();
    }
}

void BarReplayEngine::stepBackward()
{
    if (data_.isEmpty()) return;
    if (currentIndex_ > 0) {
        currentIndex_--;
        emit barChanged(currentIndex_);
    }
}

void BarReplayEngine::stop()
{
    playing_ = false;
    timer_->stop();
    currentIndex_ = 0;
    emit barChanged(currentIndex_);
}

void BarReplayEngine::setSpeed(int barsPerSecond)
{
    speed_ = std::clamp(barsPerSecond, 1, 20);
    if (playing_) {
        timer_->setInterval(std::max(50, 1000 / speed_));
    }
}

int BarReplayEngine::currentIndex() const
{
    return currentIndex_;
}

int BarReplayEngine::totalBars() const
{
    return data_.size();
}

bool BarReplayEngine::isPlaying() const
{
    return playing_;
}

QVector<QPointF> BarReplayEngine::visibleData() const
{
    if (data_.isEmpty()) return {};
    return data_.mid(0, currentIndex_ + 1);
}

void BarReplayEngine::tick()
{
    if (!playing_) return;
    if (currentIndex_ < data_.size() - 1) {
        currentIndex_++;
        emit barChanged(currentIndex_);
    }
    if (currentIndex_ >= data_.size() - 1) {
        playing_ = false;
        timer_->stop();
        emit replayFinished();
    }
}

} // namespace fincept::screens
