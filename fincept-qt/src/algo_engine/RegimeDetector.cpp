// src/algo_engine/RegimeDetector.cpp
#include "algo_engine/RegimeDetector.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::algo {

QString RegimeState::name() const { return type_name(type); }

QString RegimeState::type_name(Type t) {
    switch (t) {
        case Bull: return QStringLiteral("bull");
        case Bear: return QStringLiteral("bear");
        case Sideways: return QStringLiteral("sideways");
        case HighVolatility: return QStringLiteral("high_volatility");
        default: return QStringLiteral("unknown");
    }
}

RegimeDetector::RegimeDetector(const Config& cfg) : cfg_(cfg) {}

RegimeState RegimeDetector::detect(const QVector<double>& closes) const {
    RegimeState state;
    int n = closes.size();
    if (n < cfg_.volatility_period) return state;

    // Volatility: average true range / close
    double atr = 0.0;
    if (highs_.size() >= n && lows_.size() >= n) {
        for (int i = n - cfg_.volatility_period; i < n; ++i) {
            atr += (highs_[i] - lows_[i]) / closes[i];
        }
        atr /= cfg_.volatility_period;
        state.volatility = atr;
    }

    // Trend strength: R² of linear regression on last N closes
    {
        int m = std::min(cfg_.trend_period, n);
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
        for (int i = 0; i < m; ++i) {
            double x = i;
            double y = closes[n - m + i];
            sum_x += x; sum_y += y; sum_xy += x * y;
            sum_x2 += x * x; sum_y2 += y * y;
        }
        double denom = (m * sum_x2 - sum_x * sum_x) * (m * sum_y2 - sum_y * sum_y);
        state.trend_strength = denom > 0.0 ? std::abs(m * sum_xy - sum_x * sum_y) / std::sqrt(denom) : 0.0;
    }

    // Momentum
    if (n >= 2 && closes[n - cfg_.momentum_period] > 0) {
        state.momentum = (closes.back() - closes[n - cfg_.momentum_period]) / closes[n - cfg_.momentum_period];
    }

    // Classify regime
    bool trending = state.trend_strength >= cfg_.trend_strength_min;

    if (state.volatility >= cfg_.high_vol_threshold) {
        state.type = RegimeState::HighVolatility;
    } else if (trending && state.momentum >= cfg_.momentum_bull) {
        state.type = RegimeState::Bull;
    } else if (trending && state.momentum <= cfg_.momentum_bear) {
        state.type = RegimeState::Bear;
    } else {
        state.type = RegimeState::Sideways;
    }

    return state;
}

RegimeState RegimeDetector::update(double close, double high, double low) {
    closes_.push_back(close);
    highs_.push_back(high);
    lows_.push_back(low);

    if (closes_.size() > std::max({cfg_.volatility_period, cfg_.trend_period, cfg_.momentum_period, 50})) {
        closes_.removeFirst();
        highs_.removeFirst();
        lows_.removeFirst();
    }

    RegimeState new_state = detect(closes_);
    if (new_state.type == current_.type)
        new_state.consecutive_bars = current_.consecutive_bars + 1;
    else
        new_state.consecutive_bars = 1;

    current_ = new_state;
    return current_;
}

} // namespace fincept::algo
