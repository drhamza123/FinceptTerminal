#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QColor>

namespace fincept::screens {

struct ChartAlert {
    qint64 id = 0;
    qint64 timestamp = 0;
    double price = 0;
    QString condition;
    QString label;
    bool triggered = false;
    QColor color = QColor(255, 200, 100);
};

class ChartAlertManager : public QObject {
    Q_OBJECT
public:
    explicit ChartAlertManager(QObject* parent = nullptr);
    qint64 addAlert(const ChartAlert& alert);
    void removeAlert(qint64 id);
    void clearTriggered();
    QVector<ChartAlert> alerts() const;
    QVector<ChartAlert> activeAlerts() const;
    void checkAlerts(double currentPrice, qint64 currentTime);

signals:
    void alertTriggered(const ChartAlert& alert);
    void alertAdded(const ChartAlert& alert);
    void alertRemoved(qint64 id);

private:
    QVector<ChartAlert> alerts_;
    qint64 nextId_ = 1;
};

} // namespace fincept::screens
