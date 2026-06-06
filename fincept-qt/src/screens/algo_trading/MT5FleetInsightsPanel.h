// src/screens/algo_trading/MT5FleetInsightsPanel.h
#pragma once
#include "screens/algo_trading/MT5FleetTypes.h"
#include <QHash>
#include <QWidget>

class QComboBox; class QLabel; class QPushButton;
class QStackedWidget; class QTextBrowser; class QVBoxLayout;

namespace fincept::screens {

class MT5FleetInsightsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetInsightsPanel(QWidget* parent = nullptr);
    void set_summary(const mt5::EASummary& summary);
    void open_ai();
    void open_agent();

  signals:
    void close_requested();

  protected:
    void keyPressEvent(QKeyEvent* e) override;

  private:
    void build_ui();
    QWidget* build_ai_page();
    QWidget* build_agent_page();
    void switch_tab(int tab);
    void run_ai();
    void run_agent();
    void render_result(QTextBrowser* target, const QString& text);
    QString build_fleet_context() const;

    // Layout (same pattern as PortfolioInsightsPanel)
    QLabel* header_title_ = nullptr;
    QPushButton* header_close_btn_ = nullptr;
    QPushButton* tab_ai_btn_ = nullptr;
    QPushButton* tab_agent_btn_ = nullptr;
    QStackedWidget* pages_ = nullptr;

    // AI page
    QPushButton* ai_full_ = nullptr;
    QPushButton* ai_risk_ = nullptr;
    QPushButton* ai_perf_ = nullptr;
    QPushButton* ai_opport_ = nullptr;
    QPushButton* ai_run_ = nullptr;
    QLabel* ai_meta_ = nullptr;
    QTextBrowser* ai_content_ = nullptr;

    // Agent page
    QComboBox* agent_cb_ = nullptr;
    QPushButton* agent_run_ = nullptr;
    QLabel* agent_meta_ = nullptr;
    QTextBrowser* agent_content_ = nullptr;

    // State
    int current_tab_ = 0;
    mt5::EASummary summary_;
    bool ai_busy_ = false;
    bool agent_busy_ = false;
};

} // namespace fincept::screens
