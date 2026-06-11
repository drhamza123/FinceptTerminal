#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

class QChart;

namespace fincept::ui {

struct ChartLayout {
    QString name;
    QString timeframe;
    int chart_style = 0;       // 0=candle, 1=bar, 2=line, 3=area
    int chart_mode = 0;        // 0=candle, 1=renko, 2=kagi, 3=pnf
    bool log_scale = false;
    bool volume_footprint = false;
    QVector<QString> active_indicators;  // indicator ids
    bool show_rsi = false;
    bool show_macd = false;
    bool show_stoch = false;
    bool show_volume_profile = false;
    QString drawings_json;     // serialized drawings
    qint64 saved_at = 0;
};

class ChartLayoutManager : public QObject {
    Q_OBJECT
public:
    explicit ChartLayoutManager(QObject* parent = nullptr);

    QStringList layout_names() const { return layouts_.keys(); }
    void save_layout(const QString& name, const ChartLayout& layout);
    ChartLayout load_layout(const QString& name) const;
    void delete_layout(const QString& name);
    bool has_layout(const QString& name) const { return layouts_.contains(name); }

    // Persistence
    void load_from_disk(const QString& filepath);
    void save_to_disk(const QString& filepath) const;

signals:
    void layout_saved(const QString& name);
    void layout_deleted(const QString& name);

private:
    QHash<QString, ChartLayout> layouts_;
};

} // namespace fincept::ui
