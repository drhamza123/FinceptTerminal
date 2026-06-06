#include "screens/algo_trading/EconomicEventMarker.h"
#include <climits>
#include <cstdlib>

namespace fincept::screens {

EconomicEventOverlay::EconomicEventOverlay(QObject* parent)
    : QObject(parent)
{
}

void EconomicEventOverlay::setEvents(const QVector<EconomicEvent>& events)
{
    events_ = events;
    emit eventsChanged();
}

void EconomicEventOverlay::clear()
{
    events_.clear();
    emit eventsChanged();
}

QVector<EconomicEvent> EconomicEventOverlay::events() const
{
    return events_;
}

EconomicEvent EconomicEventOverlay::eventAt(qint64 timestamp) const
{
    if (events_.isEmpty()) return {};

    EconomicEvent nearest = events_.first();
    qint64 minDiff = std::llabs(events_.first().timestamp - timestamp);

    for (const auto& e : events_) {
        qint64 diff = std::llabs(e.timestamp - timestamp);
        if (diff < minDiff) {
            minDiff = diff;
            nearest = e;
        }
    }
    return nearest;
}

} // namespace fincept::screens
