// MT5FleetScreen.h — Fleet Center (exact copy of CryptoCenterScreen)
#pragma once
#include <QString>
#include <QWidget>

class QFrame; class QLabel; class QPushButton;
class QStackedWidget; class QTabWidget;

namespace fincept::screens {

class MT5FleetHoldingsBar;
class MT5FleetHomeTab;
class MT5FleetTradeTab;
class MT5FleetActivityTab;
class MT5FleetSettingsTab;
class MT5FleetChartPanel;
class MT5FleetOrderBookPanel;
class MT5FleetSignalsPanel;
class MT5FleetMarketplacePanel;
class MT5FleetCloudPanel;
class MT5FleetOrderPanel;
class MT5FleetFreelancePanel;
class MT5FleetVPSPanel;
class MT5FleetMarketWatchPanel;
class MT5FleetEcoCalendarPanel;
class MT5FleetMultiChartContainer;

class MT5FleetScreen : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetScreen(QWidget* parent = nullptr);
    ~MT5FleetScreen() override;
    void refreshFleet();

  private:
    void build_ui();
    void build_empty_page();
    void build_connected_page();
    void apply_theme();
    void check_connection();

    QWidget* header_ = nullptr;
    QLabel* header_brand_ = nullptr;
    QLabel* header_route_ = nullptr;
    QLabel* header_separator_ = nullptr;
    QLabel* header_status_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QWidget* empty_page_ = nullptr;
    QFrame* empty_panel_ = nullptr;
    QLabel* empty_title_ = nullptr;
    QLabel* empty_lede_ = nullptr;
    QLabel* empty_security_label_ = nullptr;
    QLabel* empty_security_text_ = nullptr;
    QPushButton* connect_button_ = nullptr;
    QWidget* connected_page_ = nullptr;
    MT5FleetHoldingsBar* holdings_bar_ = nullptr;
    QTabWidget* tab_widget_ = nullptr;
    MT5FleetHomeTab* home_tab_ = nullptr;
    MT5FleetTradeTab* trade_tab_ = nullptr;
    MT5FleetActivityTab* activity_tab_ = nullptr;
    MT5FleetSettingsTab* settings_tab_ = nullptr;
    MT5FleetChartPanel* chart_panel_ = nullptr;
    MT5FleetOrderBookPanel* orderbook_panel_ = nullptr;
    MT5FleetSignalsPanel* signals_panel_ = nullptr;
    MT5FleetMarketplacePanel* marketplace_panel_ = nullptr;
    MT5FleetCloudPanel* cloud_panel_ = nullptr;
    MT5FleetOrderPanel* order_panel_ = nullptr;
    MT5FleetFreelancePanel* freelance_panel_ = nullptr;
    MT5FleetVPSPanel* vps_panel_ = nullptr;
    MT5FleetMarketWatchPanel* market_watch_panel_ = nullptr;
    MT5FleetEcoCalendarPanel* eco_calendar_panel_ = nullptr;
    MT5FleetMultiChartContainer* multi_chart_container_ = nullptr;
};

} // namespace fincept::screens
