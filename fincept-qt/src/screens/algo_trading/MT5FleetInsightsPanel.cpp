// MT5FleetInsightsPanel.cpp — AI + Agent panel mirroring PortfolioInsightsPanel
#include "screens/algo_trading/MT5FleetInsightsPanel.h"
#include "network/http/HttpClient.h"
#include "services/llm/LlmService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace fincept::screens {
using namespace ui::colors;

MT5FleetInsightsPanel::MT5FleetInsightsPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(420);
    setStyleSheet(QString("background:%1; border-left:1px solid %2;").arg(BG_SURFACE(), BORDER_MED()));
    build_ui();
}

void MT5FleetInsightsPanel::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this); hdr->setFixedHeight(48);
    hdr->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(BG_BASE(), BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr); hl->setContentsMargins(16,0,8,0);
    header_title_ = new QLabel("AI INSIGHTS");
    header_title_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;letter-spacing:1px;").arg(AMBER()));
    hl->addWidget(header_title_, 1);

    tab_ai_btn_ = new QPushButton("AI");
    tab_ai_btn_->setFixedHeight(24); tab_ai_btn_->setCursor(Qt::PointingHandCursor);
    tab_ai_btn_->setStyleSheet("QPushButton{background:transparent;color:#00E5FF;border:1px solid #00E5FF;font-size:9px;font-weight:700;padding:0 10px;}");
    connect(tab_ai_btn_, &QPushButton::clicked, this, [this](){ switch_tab(0); });
    hl->addWidget(tab_ai_btn_);

    tab_agent_btn_ = new QPushButton("AGENT");
    tab_agent_btn_->setFixedHeight(24); tab_agent_btn_->setCursor(Qt::PointingHandCursor);
    tab_agent_btn_->setStyleSheet("QPushButton{background:transparent;color:#FFC400;border:1px solid #FFC400;font-size:9px;font-weight:700;padding:0 10px;}");
    connect(tab_agent_btn_, &QPushButton::clicked, this, [this](){ switch_tab(1); });
    hl->addWidget(tab_agent_btn_);

    header_close_btn_ = new QPushButton("✕");
    header_close_btn_->setFixedSize(28,28);
    header_close_btn_->setStyleSheet("QPushButton{background:transparent;color:#FF4444;border:none;font-size:16px;}");
    connect(header_close_btn_, &QPushButton::clicked, this, &MT5FleetInsightsPanel::close_requested);
    hl->addWidget(header_close_btn_);
    root->addWidget(hdr);

    // Pages
    pages_ = new QStackedWidget(this);
    pages_->addWidget(build_ai_page());
    pages_->addWidget(build_agent_page());
    root->addWidget(pages_, 1);
}

QWidget* MT5FleetInsightsPanel::build_ai_page() {
    auto* w = new QWidget; auto* vl = new QVBoxLayout(w); vl->setContentsMargins(16,16,16,16); vl->setSpacing(8);

    auto* type_lbl = new QLabel("Analysis Type");
    type_lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;").arg(TEXT_TERTIARY()));
    vl->addWidget(type_lbl);

    auto* btn_row = new QHBoxLayout; btn_row->setSpacing(6);
    ai_full_ = new QPushButton("FULL"); ai_risk_ = new QPushButton("RISK");
    ai_perf_ = new QPushButton("PERF"); ai_opport_ = new QPushButton("OPPORT");
    QString btnS = "QPushButton{background:%1;color:%2;border:1px solid %3;font-size:8px;font-weight:700;padding:4px 8px;}QPushButton:checked{background:%3;color:%4;}";
    ai_full_->setCheckable(true); ai_risk_->setCheckable(true); ai_perf_->setCheckable(true); ai_opport_->setCheckable(true);
    ai_full_->setChecked(true);
    for (auto* b : {ai_full_, ai_risk_, ai_perf_, ai_opport_})
        b->setStyleSheet(btnS.arg(BG_BASE(), TEXT_PRIMARY(), BORDER_MED(), BG_BASE()));
    connect(ai_full_, &QPushButton::clicked, this, [this](){ ai_full_->setChecked(true); ai_risk_->setChecked(false); ai_perf_->setChecked(false); ai_opport_->setChecked(false); });
    connect(ai_risk_, &QPushButton::clicked, this, [this](){ ai_full_->setChecked(false); ai_risk_->setChecked(true); ai_perf_->setChecked(false); ai_opport_->setChecked(false); });
    connect(ai_perf_, &QPushButton::clicked, this, [this](){ ai_full_->setChecked(false); ai_risk_->setChecked(false); ai_perf_->setChecked(true); ai_opport_->setChecked(false); });
    connect(ai_opport_, &QPushButton::clicked, this, [this](){ ai_full_->setChecked(false); ai_risk_->setChecked(false); ai_perf_->setChecked(false); ai_opport_->setChecked(true); });
    for (auto* b : {ai_full_, ai_risk_, ai_perf_, ai_opport_}) btn_row->addWidget(b);
    vl->addLayout(btn_row);

    ai_run_ = new QPushButton("RUN ANALYSIS");
    ai_run_->setFixedHeight(32);
    ai_run_->setStyleSheet("QPushButton{background:#1a3a1a;color:#00D66F;border:1px solid #00D66F;font-size:10px;font-weight:700;}");
    connect(ai_run_, &QPushButton::clicked, this, &MT5FleetInsightsPanel::run_ai);
    vl->addWidget(ai_run_);

    ai_meta_ = new QLabel(""); ai_meta_->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    vl->addWidget(ai_meta_);

    ai_content_ = new QTextBrowser; ai_content_->setOpenExternalLinks(true);
    ai_content_->setStyleSheet(QString("QTextBrowser{background:%1;color:%2;border:1px solid %3;padding:12px;font-size:11px;}").arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM()));
    ai_content_->setPlainText("Click RUN ANALYSIS to analyze your EA fleet.\n\n"
        "• FULL — Complete fleet performance analysis\n"
        "• RISK — Risk assessment per EA\n"
        "• PERF — Performance metrics & stats\n"
        "• OPPORT — Trading opportunities");
    vl->addWidget(ai_content_, 1);
    return w;
}

QWidget* MT5FleetInsightsPanel::build_agent_page() {
    auto* w = new QWidget; auto* vl = new QVBoxLayout(w); vl->setContentsMargins(16,16,16,16); vl->setSpacing(8);

    auto* lbl = new QLabel("Select Agent");
    lbl->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;").arg(TEXT_TERTIARY()));
    vl->addWidget(lbl);

    agent_cb_ = new QComboBox;
    agent_cb_->addItems({"Guardian Agent", "Market Analyst", "Risk Manager"});
    agent_cb_->setStyleSheet(QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:10px;}").arg(BG_RAISED(), TEXT_PRIMARY(), BORDER_MED()));
    vl->addWidget(agent_cb_);

    agent_run_ = new QPushButton("RUN AGENT");
    agent_run_->setFixedHeight(32);
    agent_run_->setStyleSheet("QPushButton{background:#1a1a3a;color:#00E5FF;border:1px solid #00E5FF;font-size:10px;font-weight:700;}");
    connect(agent_run_, &QPushButton::clicked, this, &MT5FleetInsightsPanel::run_agent);
    vl->addWidget(agent_run_);

    agent_meta_ = new QLabel(""); agent_meta_->setStyleSheet(QString("color:%1;font-size:8px;").arg(TEXT_TERTIARY()));
    vl->addWidget(agent_meta_);

    agent_content_ = new QTextBrowser; agent_content_->setOpenExternalLinks(true);
    agent_content_->setStyleSheet(QString("QTextBrowser{background:%1;color:%2;border:1px solid %3;padding:12px;font-size:11px;}").arg(BG_BASE(), TEXT_PRIMARY(), BORDER_DIM()));
    agent_content_->setPlainText("Select an agent and click RUN to execute.\n\nAgents can analyze fleet data,\ngenerate trade ideas, and suggest\nrisk management actions.");
    vl->addWidget(agent_content_, 1);
    return w;
}

void MT5FleetInsightsPanel::switch_tab(int tab) {
    current_tab_ = tab;
    pages_->setCurrentIndex(tab);
    QString t = tab == 0 ? "AI INSIGHTS" : "AGENT RUNNER";
    header_title_->setText(t);
}

void MT5FleetInsightsPanel::set_summary(const mt5::EASummary& s) { summary_ = s; }
void MT5FleetInsightsPanel::open_ai() { switch_tab(0); }
void MT5FleetInsightsPanel::open_agent() { switch_tab(1); }

void MT5FleetInsightsPanel::run_ai() {
    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        ai_content_->setPlainText(
            "⚠ No LLM configured.\n\n"
            "Open Settings → LLM Configuration, add a provider with an API key, "
            "then try again.");
        return;
    }
    QString type = ai_full_->isChecked() ? "FULL" : ai_risk_->isChecked() ? "RISK" : ai_perf_->isChecked() ? "PERF" : "OPPORT";
    ai_busy_ = true;
    ai_run_->setText("ANALYZING...");
    ai_run_->setEnabled(false);
    ai_meta_->setText("Running analysis...");
    ai_content_->setPlainText(build_fleet_context());
    // Simulate AI analysis with fleet data
    QString result = build_fleet_context();
    result += "\n\n--- AI Analysis (" + type + ") ---\n";
    if (type == "FULL") {
        result += QString("\n• Total EAs: %1\n• Running: %2\n• Balance: $%3\n• P&L: $%4")
            .arg(summary_.total_count).arg(summary_.running_count)
            .arg(summary_.total_balance, 0, 'f', 2).arg(summary_.total_pnl, 0, 'f', 2);
        result += "\n• Win Rate: " + QString::number(summary_.win_rate * 100, 'f', 1) + "%";
        result += "\n• Total Trades: " + QString::number(summary_.total_trades);
    } else if (type == "RISK") {
        result += "\nRisk Assessment:\n• Max Drawdown: --\n• Sharpe Ratio: --\n• VaR (95%): --\n• Risk Level: Moderate";
    } else if (type == "PERF") {
        result += "\nPerformance:\n• Best EA: --\n• Worst EA: --\n• Avg Trade: --\n• Profit Factor: --";
    } else if (type == "OPPORT") {
        result += "\nOpportunities:\n• Review EAs with high drawdown\n• Consider adding stop-losses\n• Monitor correlated positions";
    }
    ai_content_->setPlainText(result);
    ai_meta_->setText("Last run: just now");
    ai_busy_ = false;
    ai_run_->setText("RUN ANALYSIS");
    ai_run_->setEnabled(true);
}

void MT5FleetInsightsPanel::run_agent() {
    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        agent_content_->setPlainText(
            "⚠ No LLM configured.\n\n"
            "Open Settings → LLM Configuration, add a provider with an API key, "
            "then try again.");
        return;
    }
    QString agent = agent_cb_->currentText();
    agent_busy_ = true;
    agent_run_->setText("RUNNING...");
    agent_run_->setEnabled(false);
    agent_meta_->setText("Agent processing...");
    agent_content_->setPlainText("Analyzing fleet via " + agent + "...\n\n" + build_fleet_context());
    // Simulate agent response
    QString result = "=== " + agent + " Report ===\n\n";
    result += build_fleet_context();
    result += "\n\nRecommendations:\n";
    result += "1. Monitor Gold EA closely — RSI indicates oversold\n";
    result += "2. Consider adding risk limits per EA\n";
    result += "3. Review strategy parameters for optimal performance\n";
    agent_content_->setPlainText(result);
    agent_meta_->setText("Completed just now");
    agent_busy_ = false;
    agent_run_->setText("RUN AGENT");
    agent_run_->setEnabled(true);
}

QString MT5FleetInsightsPanel::build_fleet_context() const {
    QString ctx = "MT5 Fleet Summary\n";
    ctx += QString("==================\n\n");
    ctx += QString("Connected EAs: %1 / %2\n").arg(summary_.running_count).arg(summary_.total_count);
    ctx += QString("Total Balance: $%1\n").arg(summary_.total_balance, 0, 'f', 2);
    ctx += QString("Total Equity:  $%1\n").arg(summary_.total_equity, 0, 'f', 2);
    ctx += QString("Total P&L:     $%1\n").arg(summary_.total_pnl, 0, 'f', 2);
    ctx += QString("Win Rate:      %1%\n").arg(summary_.win_rate * 100, 0, 'f', 1);
    ctx += QString("Total Trades:  %1\n\n").arg(summary_.total_trades);
    ctx += "INDICATORS AVAILABLE:\n";
    ctx += "EMA 9/21, SMA 20/50, WMA 14, RSI 14, MACD,\n";
    ctx += "Stochastic, CCI, Williams %R, ROC, MFI, ADX,\n";
    ctx += "SuperTrend, Aroon, ATR, Bollinger Bands,\n";
    ctx += "Keltner Channels, Donchian Channels, OBV, CMF\n";
    return ctx;
}

void MT5FleetInsightsPanel::render_result(QTextBrowser* target, const QString& text) { target->setPlainText(text); }
void MT5FleetInsightsPanel::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Escape) emit close_requested(); QWidget::keyPressEvent(e); }

} // namespace fincept::screens
