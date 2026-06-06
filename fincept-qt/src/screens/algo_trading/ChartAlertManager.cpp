#include "screens/algo_trading/ChartAlertManager.h"
#include <algorithm>

namespace fincept::screens {

ChartAlertManager::ChartAlertManager(QObject* parent)
    : QObject(parent)
{
}

qint64 ChartAlertManager::addAlert(const ChartAlert& alert)
{
    ChartAlert a = alert;
    a.id = nextId_++;
    alerts_.append(a);
    emit alertAdded(a);
    return a.id;
}

void ChartAlertManager::removeAlert(qint64 id)
{
    auto it = std::find_if(alerts_.begin(), alerts_.end(), [id](const ChartAlert& a) {
        return a.id == id;
    });
    if (it != alerts_.end()) {
        alerts_.erase(it);
        emit alertRemoved(id);
    }
}

void ChartAlertManager::clearTriggered()
{
    alerts_.erase(std::remove_if(alerts_.begin(), alerts_.end(), [](const ChartAlert& a) {
        return a.triggered;
    }), alerts_.end());
}

QVector<ChartAlert> ChartAlertManager::alerts() const
{
    return alerts_;
}

QVector<ChartAlert> ChartAlertManager::activeAlerts() const
{
    QVector<ChartAlert> active;
    for (const auto& a : alerts_) {
        if (!a.triggered) {
            active.append(a);
        }
    }
    return active;
}

void ChartAlertManager::checkAlerts(double currentPrice, qint64 currentTime)
{
    Q_UNUSED(currentTime);
    for (auto& a : alerts_) {
        if (a.triggered) continue;

        bool triggered = false;
        if (a.condition == "above" && currentPrice >= a.price) {
            triggered = true;
        } else if (a.condition == "below" && currentPrice <= a.price) {
            triggered = true;
        } else if (a.condition == "crosses") {
            triggered = true;
        }

        if (triggered) {
            a.triggered = true;
            emit alertTriggered(a);
        }
    }
}

} // namespace fincept::screens
