// src/algo_engine/MlSignalBridge.h
#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"

namespace fincept::algo {

/// Bridge between ML models (trained in Quant Lab Python) and live trading signals.
struct MlSignal {
    double direction = 0.0;    // -1 (sell) to +1 (buy)
    double confidence = 0.0;   // 0-1
    double prediction = 0.0;
    QString model_name;
    QString model_type;
};

class MlSignalBridge {
public:
    MlSignalBridge() = default;

    void configure(const QString& model_path, const QString& model_type,
                   const QStringList& features) {
        model_path_ = model_path;
        model_type_ = model_type;
        features_ = features;
    }

    bool is_ready() const { return !model_path_.isEmpty(); }

    MlSignal predict(const QVector<double>& ohlc_features) {
        if (!is_ready()) {
            LOG_WARN("MlSignal", "No model configured, returning neutral");
            return {};
        }
        QJsonObject req;
        req["model_path"] = model_path_;
        req["model_type"] = model_type_;
        QJsonArray feats;
        for (double v : ohlc_features) feats.append(v);
        req["features"] = feats;
        if (!features_.isEmpty()) {
            QJsonArray names;
            for (const auto& n : features_) names.append(n);
            req["feature_names"] = names;
        }
        QString script = "ml_inference.py";
        QStringList args;
        args << QString::fromUtf8(QJsonDocument(req).toJson(QJsonDocument::Compact));
        python::PythonRunner::instance().run(script, args,
            [this](python::PythonResult result) {});
        return {};
    }

    QString model_path() const { return model_path_; }
    QString model_type() const { return model_type_; }
    QStringList features() const { return features_; }

private:
    QString model_path_;
    QString model_type_;
    QStringList features_;
};

} // namespace fincept::algo
