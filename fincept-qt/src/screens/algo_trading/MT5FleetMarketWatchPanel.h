// MT5FleetMarketWatchPanel.h — Real-time Market Watch with bid/ask/spread
#pragma once
#include <QWidget>
#include <QTimer>
#include <QMap>
#include <QSet>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;
class QLabel;

namespace fincept::screens {

struct MarketWatchEntry {
    QString symbol;
    double bid = 0;
    double ask = 0;
    double spread = 0;
    double last = 0;
    double high = 0;
    double low = 0;
    double change = 0;
    double change_pct = 0;
    double volume = 0;
    QString category;
    bool up = false;
    bool changed = false;
};

class MT5FleetMarketWatchPanel : public QWidget {
    Q_OBJECT
public:
    explicit MT5FleetMarketWatchPanel(QWidget* parent = nullptr);
    ~MT5FleetMarketWatchPanel() override;
    QString currentSymbol() const { return current_symbol_; }
    void setSymbols(const QStringList& symbols);

signals:
    void symbolSelected(const QString& symbol);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private slots:
    void refresh_all();
    void on_search_changed(const QString& text);
    void on_filter_changed(int idx);
    void on_item_clicked(QTreeWidgetItem* item, int col);

private:
    void build_ui();
    void apply_theme();
    void fetch_quote(const QString& symbol);
    void update_entry(const QString& symbol, MarketWatchEntry entry);
    void repopulate_tree();
    QString category_icon(const QString& cat) const;
    QString change_arrow(double change) const;

    // Known symbols by category
    QStringList forex_symbols_;
    QStringList crypto_symbols_;
    QStringList stock_symbols_;
    QStringList commodity_symbols_;
    QStringList all_symbols_;

    QMap<QString, MarketWatchEntry> entries_;
    QSet<QString> active_requests_;

    QLineEdit* search_input_ = nullptr;
    QComboBox* filter_combo_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* timer_label_ = nullptr;

    QTimer* timer_ = nullptr;
    int refresh_count_ = 0;
    int fetch_index_ = 0;
    bool is_active_ = false;
    QString current_symbol_;
    QString search_filter_;
    QString category_filter_ = "All";
};

} // namespace fincept::screens
