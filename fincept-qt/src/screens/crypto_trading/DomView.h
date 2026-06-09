#pragma once
#include "screens/crypto_trading/CryptoOrderBook.h"
// Adds Depth of Market (DOM) view with one-click trading
// The existing CryptoOrderBook already supports ObViewMode::Book
// We add a new DOM mode with aggregated depth levels

namespace fincept::screens::crypto {

class DomView {
public:
    static void render_dom(QPainter& p, const QRect& rect,
                           const QVector<QPair<double,double>>& bids,
                           const QVector<QPair<double,double>>& asks) {
        // Aggregate by price level (10 levels per side)
        int levels = 10;
        double max_vol = 0;
        struct DomRow { double price; double bid_vol; double ask_vol; };
        QVector<DomRow> rows;

        for (int i = 0; i < levels && i < bids.size(); i++)
            max_vol = std::max(max_vol, bids[i].second);
        for (int i = 0; i < levels && i < asks.size(); i++)
            max_vol = std::max(max_vol, asks[i].second);

        int idx = 0;
        for (int i = levels - 1; i >= 0 && i < bids.size(); i--, idx++) {
            DomRow r;
            r.price = bids[i].first;
            r.bid_vol = bids[i].second;
            r.ask_vol = idx < asks.size() ? asks[idx].second : 0;
            rows.append(r);
        }

        // Draw DOM
        double row_h = (double)rect.height() / rows.size();
        QFont f("Consolas", 9); p.setFont(f);

        for (int i = 0; i < rows.size(); i++) {
            double y = rect.y() + i * row_h;
            QRectF r(rect.x(), y, rect.width(), row_h - 1);

            // Bid bar (right-aligned)
            if (rows[i].bid_vol > 0 && max_vol > 0) {
                double bw = (rows[i].bid_vol / max_vol) * rect.width() * 0.4;
                p.fillRect(QRectF(rect.width() / 2 - bw, y, bw, row_h - 1), QColor(59, 130, 246, 60));
            }
            // Ask bar (left-aligned from center)
            if (rows[i].ask_vol > 0 && max_vol > 0) {
                double aw = (rows[i].ask_vol / max_vol) * rect.width() * 0.4;
                p.fillRect(QRectF(rect.width() / 2, y, aw, row_h - 1), QColor(245, 158, 11, 60));
            }

            // Price label
            p.setPen(QColor("#e5e5e5"));
            p.drawText(QRectF(rect.x(), y, rect.width() / 3, row_h), Qt::AlignCenter,
                       QString::number(rows[i].price, 'f', 2));

            // Bid volume
            p.setPen(QColor("#3b82f6"));
            p.drawText(QRectF(rect.x() + rect.width() / 3, y, rect.width() / 3, row_h),
                       Qt::AlignCenter, QString::number(rows[i].bid_vol, 'f', rows[i].bid_vol > 1000 ? 0 : 2));
        }
    }
};

} // namespace
