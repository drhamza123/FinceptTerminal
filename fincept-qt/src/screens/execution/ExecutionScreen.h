#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QWebSocket>
#include "screens/algo_trading/MT5FleetChartPanel.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"
#include "screens/crypto_trading/CryptoBottomPanel.h"
#include "screens/crypto_trading/CryptoTickerBar.h"
#include "screens/crypto_trading/CryptoWatchlist.h"

namespace fincept::screens {

class ExecutionScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ExecutionScreen(QWidget* parent = nullptr);
    MT5FleetChartPanel* chart() const { return chart_; }

  private slots:
    void refresh_account();
    void on_symbol_submit();
    void on_symbol_selected(const QString& symbol);
    void on_mode_toggled();

  private:
    void build_ui();

    MT5FleetChartPanel* chart_ = nullptr;
    crypto::CryptoTickerBar* ticker_ = nullptr;
    crypto::CryptoWatchlist* watchlist_ = nullptr;
    crypto::CryptoOrderBook* orderbook_ = nullptr;
    crypto::CryptoOrderEntry* orderentry_ = nullptr;
    crypto::CryptoBottomPanel* bottom_ = nullptr;

    // Wallet
    QLabel* bal_lbl_ = nullptr;
    QLabel* eq_lbl_ = nullptr;
    QLabel* bp_lbl_ = nullptr;
    QLabel* pnl_lbl_ = nullptr;

    // Command bar
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* mode_btn_ = nullptr;
    QLabel* ws_status_ = nullptr;
    bool is_paper_mode_ = true;
};

} // namespace fincept::screens
