#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace fincept::ui {

struct ChartAlert {
    enum Type { PriceAbove, PriceBelow, CrossingAbove, CrossingBelow };
    enum Status { Active, Triggered, Disabled };

    QString id;
    QString symbol;
    QString label;
    Type type = PriceAbove;
    double price = 0;
    double current_value = 0;
    Status status = Active;
    bool once = true;
    bool sound_enabled = false;
    qint64 created_at = 0;
    qint64 triggered_at = 0;
    int frequency_sec = 0;
};

class AlertManager : public QObject {
    Q_OBJECT
public:
    explicit AlertManager(QObject* parent = nullptr);

    QString add_alert(const ChartAlert& alert);
    void remove_alert(const QString& id);
    void clear_all();

    void check_alerts(double current_price);
    void set_alerts(const QVector<ChartAlert>& alerts);

    const QVector<ChartAlert>& alerts() const { return alerts_; }

signals:
    void alert_triggered(const ChartAlert& alert);
    void alert_added(const QString& id);
    void alert_removed(const QString& id);

private:
    QVector<ChartAlert> alerts_;
    int next_id_ = 1;
};

} // namespace fincept::ui
