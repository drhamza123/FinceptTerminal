#include "ui/charts/ChartLayoutManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace fincept::ui {

ChartLayoutManager::ChartLayoutManager(QObject* parent) : QObject(parent) {}

void ChartLayoutManager::save_layout(const QString& name, const ChartLayout& layout) {
    ChartLayout copy = layout;
    copy.name = name;
    copy.saved_at = QDateTime::currentMSecsSinceEpoch();
    layouts_[name] = copy;
    emit layout_saved(name);
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/chart_layouts/";
    QDir().mkpath(path);
    save_to_disk(path + name + ".json");
}

ChartLayout ChartLayoutManager::load_layout(const QString& name) const {
    return layouts_.value(name);
}

void ChartLayoutManager::delete_layout(const QString& name) {
    layouts_.remove(name);
    emit layout_deleted(name);
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/chart_layouts/" + name + ".json";
    QFile::remove(path);
}

void ChartLayoutManager::load_from_disk(const QString& filepath) {
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;
    auto root = doc.object();
    ChartLayout l;
    l.name = QFileInfo(filepath).baseName();
    l.timeframe = root["timeframe"].toString();
    l.chart_style = root["chart_style"].toInt();
    l.chart_mode = root["chart_mode"].toInt();
    l.log_scale = root["log_scale"].toBool();
    l.volume_footprint = root["volume_footprint"].toBool();
    l.show_rsi = root["show_rsi"].toBool();
    l.show_macd = root["show_macd"].toBool();
    l.show_stoch = root["show_stoch"].toBool();
    l.show_volume_profile = root["show_volume_profile"].toBool();
    l.drawings_json = root["drawings"].toString();
    l.saved_at = static_cast<qint64>(root["saved_at"].toDouble());
    for (const auto& v : root["indicators"].toArray())
        l.active_indicators.append(v.toString());
    layouts_[l.name] = l;
}

void ChartLayoutManager::save_to_disk(const QString& filepath) const {
    QString name = QFileInfo(filepath).baseName();
    auto it = layouts_.find(name);
    if (it == layouts_.end()) return;
    const auto& l = it.value();

    QJsonObject root;
    root["timeframe"] = l.timeframe;
    root["chart_style"] = l.chart_style;
    root["chart_mode"] = l.chart_mode;
    root["log_scale"] = l.log_scale;
    root["volume_footprint"] = l.volume_footprint;
    root["show_rsi"] = l.show_rsi;
    root["show_macd"] = l.show_macd;
    root["show_stoch"] = l.show_stoch;
    root["show_volume_profile"] = l.show_volume_profile;
    root["drawings"] = l.drawings_json;
    root["saved_at"] = static_cast<double>(l.saved_at);
    QJsonArray inds;
    for (const auto& i : l.active_indicators) inds.append(i);
    root["indicators"] = inds;

    QFile f(filepath);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

} // namespace fincept::ui
