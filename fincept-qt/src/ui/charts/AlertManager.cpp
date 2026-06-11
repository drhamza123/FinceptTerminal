#include "ui/charts/AlertManager.h"

#include <QDateTime>

namespace fincept::ui {

AlertManager::AlertManager(QObject* parent) : QObject(parent) {}

QString AlertManager::add_alert(const ChartAlert& alert) {
    ChartAlert copy = alert;
    copy.id = QString("alert_%1").arg(next_id_++);
    copy.created_at = QDateTime::currentMSecsSinceEpoch();
    alerts_.append(copy);
    emit alert_added(copy.id);
    return copy.id;
}

void AlertManager::remove_alert(const QString& id) {
    for (int i = 0; i < alerts_.size(); ++i) {
        if (alerts_[i].id == id) {
            alerts_.removeAt(i);
            emit alert_removed(id);
            return;
        }
    }
}

void AlertManager::clear_all() {
    alerts_.clear();
}

void AlertManager::set_alerts(const QVector<ChartAlert>& alerts) {
    alerts_ = alerts;
}

void AlertManager::check_alerts(double current_price) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (auto& alert : alerts_) {
        if (alert.status != ChartAlert::Active) continue;

        bool triggered = false;

        switch (alert.type) {
        case ChartAlert::PriceAbove:
            triggered = current_price >= alert.price;
            break;
        case ChartAlert::PriceBelow:
            triggered = current_price <= alert.price;
            break;
        case ChartAlert::CrossingAbove:
            triggered = alert.current_value < alert.price && current_price >= alert.price;
            break;
        case ChartAlert::CrossingBelow:
            triggered = alert.current_value > alert.price && current_price <= alert.price;
            break;
        }

        alert.current_value = current_price;

        if (triggered) {
            alert.status = ChartAlert::Triggered;
            alert.triggered_at = now;

            if (alert.once) {
                emit alert_triggered(alert);
            } else {
                emit alert_triggered(alert);
                // Recurring: re-activate after frequency_sec
                if (alert.frequency_sec > 0) {
                    alert.status = ChartAlert::Active;
                }
            }
        }
    }
}

} // namespace fincept::ui
