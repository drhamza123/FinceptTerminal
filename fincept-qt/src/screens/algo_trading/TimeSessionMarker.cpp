#include "screens/algo_trading/TimeSessionMarker.h"

namespace fincept::screens {

TimeSessionManager::TimeSessionManager(QObject* parent)
    : QObject(parent)
{
}

void TimeSessionManager::setSessions(const QVector<TimeSession>& sessions)
{
    sessions_ = sessions;
    emit sessionsChanged();
}

QVector<TimeSession> TimeSessionManager::sessions() const
{
    return sessions_;
}

void TimeSessionManager::generateDailySessions(qint64 date)
{
    constexpr qint64 secsPerHour = 3600;
    qint64 openTime = date + 9 * secsPerHour + 30 * 60;
    qint64 closeTime = date + 16 * secsPerHour;

    sessions_.clear();

    TimeSession pre;
    pre.type = TimeSession::PreMarket;
    pre.startTime = openTime - 4 * secsPerHour;
    pre.endTime = openTime;
    pre.color = QColor(100, 100, 255, 40);
    sessions_.append(pre);

    TimeSession reg;
    reg.type = TimeSession::Regular;
    reg.startTime = openTime;
    reg.endTime = closeTime;
    reg.color = QColor(0, 0, 0, 0);
    sessions_.append(reg);

    TimeSession after;
    after.type = TimeSession::AfterHours;
    after.startTime = closeTime;
    after.endTime = closeTime + 4 * secsPerHour;
    after.color = QColor(255, 100, 100, 40);
    sessions_.append(after);

    emit sessionsChanged();
}

void TimeSessionManager::clear()
{
    sessions_.clear();
    emit sessionsChanged();
}

} // namespace fincept::screens
