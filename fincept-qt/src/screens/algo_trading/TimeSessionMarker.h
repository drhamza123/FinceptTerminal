#pragma once
#include <QObject>
#include <QVector>
#include <QColor>

namespace fincept::screens {

struct TimeSession {
    enum Type { PreMarket, Regular, AfterHours };
    Type type;
    qint64 startTime;
    qint64 endTime;
    QColor color;
};

class TimeSessionManager : public QObject {
    Q_OBJECT
public:
    explicit TimeSessionManager(QObject* parent = nullptr);
    void setSessions(const QVector<TimeSession>& sessions);
    QVector<TimeSession> sessions() const;
    void generateDailySessions(qint64 date);
    void clear();

signals:
    void sessionsChanged();

private:
    QVector<TimeSession> sessions_;
};

} // namespace fincept::screens
