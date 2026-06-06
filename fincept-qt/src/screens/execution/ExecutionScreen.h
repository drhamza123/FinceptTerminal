#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QSplitter>
#include "screens/algo_trading/MT5FleetChartPanel.h"
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "screens/crypto_trading/CryptoOrderEntry.h"

namespace fincept::screens {

class ExecutionScreen : public QWidget {
    Q_OBJECT
  public:
    explicit ExecutionScreen(QWidget* parent = nullptr);
    MT5FleetChartPanel* chart() const { return chart_; }

  private:
    MT5FleetChartPanel* chart_ = nullptr;
    crypto::CryptoOrderBook* orderbook_ = nullptr;
    crypto::CryptoOrderEntry* orderentry_ = nullptr;
};

} // namespace fincept::screens
