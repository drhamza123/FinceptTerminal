#pragma once
#include <QObject>
#include <QVector>
#include <QString>

namespace fincept::screens {

struct EconomicEvent {
    qint64 timestamp;
    QString title;
    QString impact;
    QString actual;
    QString forecast;
    QString previous;
};

class EconomicEventOverlay : public QObject {
    Q_OBJECT
public:
    explicit EconomicEventOverlay(QObject* parent = nullptr);
    void setEvents(const QVector<EconomicEvent>& events);
    void clear();
    QVector<EconomicEvent> events() const;
    EconomicEvent eventAt(qint64 timestamp) const;

signals:
    void eventsChanged();

private:
    QVector<EconomicEvent> events_;
};

} // namespace fincept::screens
