#include "screens/community/CommunityMarketplaceScreen.h"
#include "core/config/AppConfig.h"
#include "ui/theme/Theme.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QNetworkRequest>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QApplication>
#include <QClipboard>

namespace fincept::screens {

CommunityMarketplaceScreen::CommunityMarketplaceScreen(QWidget* parent) : QWidget(parent) {
    net_ = new QNetworkAccessManager(this);
    base_url_ = fincept::AppConfig::instance().api_base_url() + QStringLiteral("/community");
    build_ui();
    fetch_tools();
    fetch_agents();
}

void CommunityMarketplaceScreen::build_ui() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(12,12,12,12);

    auto* title = new QLabel("<h2>Community Marketplace</h2>"
        "<p style='color:#808080;'>Discover and install MCP tools & agent configurations.</p>", this);
    root->addWidget(title);

    auto* bar = new QHBoxLayout();
    search_edit_ = new QLineEdit(this); search_edit_->setPlaceholderText("Search tools...");
    search_edit_->setStyleSheet("background:#1a1a2e;color:#e5e5e5;border:1px solid #2a2a3e;padding:6px 12px;");
    connect(search_edit_, &QLineEdit::returnPressed, this, &CommunityMarketplaceScreen::on_search);
    bar->addWidget(search_edit_, 1);

    category_combo_ = new QComboBox(this);
    category_combo_->addItems({"All", "Data", "Trading", "AI", "Utility"});
    category_combo_->setStyleSheet("background:#1a1a2e;color:#e5e5e5;border:1px solid #2a2a3e;padding:6px;");
    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CommunityMarketplaceScreen::on_category_changed);
    bar->addWidget(category_combo_);

    auto* refresh_btn = new QPushButton("Refresh", this);
    refresh_btn->setStyleSheet("background:#22c55e;color:#080808;border:none;padding:6px 16px;font-weight:700;");
    connect(refresh_btn, &QPushButton::clicked, this, [this](){ fetch_tools(); fetch_agents(); });
    bar->addWidget(refresh_btn);
    root->addLayout(bar);

    tabs_ = new QTabWidget(this);
    tabs_->setStyleSheet("QTabWidget::pane{background:#0f0f1e;border:1px solid #2a2a3e;}"
                         "QTabBar::tab{background:#1a1a2e;color:#808080;padding:8px 16px;border:1px solid #2a2a3e;}"
                         "QTabBar::tab:selected{background:#0f0f1e;color:#22c55e;}");

    // Tools tab
    tools_tab_ = new QWidget(this);
    tools_layout_ = new QVBoxLayout(tools_tab_); tools_layout_->setContentsMargins(8,8,8,8);
    auto* tools_scroll = new QScrollArea(this); tools_scroll->setWidget(tools_tab_);
    tools_scroll->setWidgetResizable(true); tools_scroll->setFrameShape(QFrame::NoFrame);
    tabs_->addTab(tools_scroll, "MCP Tools");

    // Agents tab
    agents_tab_ = new QWidget(this);
    agents_layout_ = new QVBoxLayout(agents_tab_); agents_layout_->setContentsMargins(8,8,8,8);
    auto* agents_scroll = new QScrollArea(this); agents_scroll->setWidget(agents_tab_);
    agents_scroll->setWidgetResizable(true); agents_scroll->setFrameShape(QFrame::NoFrame);
    tabs_->addTab(agents_scroll, "Agent Configs");

    root->addWidget(tabs_, 1);
}

QWidget* CommunityMarketplaceScreen::build_tool_row(const QJsonObject& tool) {
    auto* card = new QFrame(this);
    card->setStyleSheet("QFrame{background:#1a1a2e;border:1px solid #2a2a3e;border-radius:6px;margin:4px 0;}");
    auto* hl = new QHBoxLayout(card); hl->setContentsMargins(12,10,12,10);

    auto* info = new QVBoxLayout();
    auto* name_lbl = new QLabel(QString("<b style='color:#e5e5e5;font-size:14px;'>%1</b>"
        " <span style='color:#22c55e;font-size:11px;'>v%2</span>")
        .arg(tool["name"].toString(), tool["version"].toString()), this);
    info->addWidget(name_lbl);
    auto* desc = new QLabel(tool["description"].toString(), this);
    desc->setStyleSheet("color:#808080;font-size:11px;");
    desc->setWordWrap(true);
    info->addWidget(desc);
    auto* meta = new QLabel(QString("by %1 · %2 · %3 downloads")
        .arg(tool["author"].toString(), tool["category"].toString())
        .arg(tool["downloads"].toInt()), this);
    meta->setStyleSheet("color:#555;font-size:10px;");
    info->addWidget(meta);
    hl->addLayout(info, 1);

    auto* install_btn = new QPushButton("Install", this);
    install_btn->setStyleSheet("background:#22c55e;color:#080808;border:none;padding:8px 20px;font-weight:700;border-radius:4px;"
                               "QPushButton:hover{background:#16a34a;}");
    QString tid = tool["id"].toString();
    connect(install_btn, &QPushButton::clicked, this, [this, tid]() { install_tool(tid); });
    hl->addWidget(install_btn);

    return card;
}

void CommunityMarketplaceScreen::fetch_tools() {
    QString url = base_url_ + "/tools";
    QString cat = category_combo_->currentText();
    if (cat != "All") url += "?category=" + cat;
    QString q = search_edit_->text().trimmed();
    if (!q.isEmpty()) url += (cat == "All" ? "?" : "&") + QString("search=") + q;
    QNetworkReply* reply = net_->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, &CommunityMarketplaceScreen::on_tools_fetched);
}

void CommunityMarketplaceScreen::fetch_agents() {
    QNetworkReply* reply = net_->get(QNetworkRequest(QUrl(base_url_ + "/agents")));
    connect(reply, &QNetworkReply::finished, this, &CommunityMarketplaceScreen::on_agents_fetched);
}

void CommunityMarketplaceScreen::on_search() { fetch_tools(); }
void CommunityMarketplaceScreen::on_category_changed(int) { fetch_tools(); }

void CommunityMarketplaceScreen::on_tools_fetched() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || reply->error() != QNetworkReply::NoError) {
        QLabel* err = new QLabel("Backend unreachable — start the API server on port 8150", this);
        err->setStyleSheet("color:#ff6b6b;padding:20px;");
        tools_layout_->addWidget(err);
        reply->deleteLater();
        return;
    }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray tools = doc.array();
    while (auto* item = tools_layout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    if (tools.isEmpty()) {
        tools_layout_->addWidget(new QLabel("No tools found. Publish one via the API!", this));
    }
    for (const auto& v : tools) {
        tools_layout_->addWidget(build_tool_row(v.toObject()));
    }
    reply->deleteLater();
}

void CommunityMarketplaceScreen::on_agents_fetched() {
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply || reply->error() != QNetworkReply::NoError) { if (reply) reply->deleteLater(); return; }
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray agents = doc.array();
    while (auto* item = agents_layout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    for (const auto& v : agents) {
        auto obj = v.toObject();
        auto* card = build_tool_row(obj); // Reuse same layout
        agents_layout_->addWidget(card);
    }
    reply->deleteLater();
}

void CommunityMarketplaceScreen::install_tool(const QString& toolId) {
    auto reply = QMessageBox::question(this, "Security Warning",
        "Installing community tools executes Python code. "
        "Only install from trusted authors. Continue?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QUrl url(base_url_ + "/tools/" + toolId + "/install");
    QNetworkRequest req(url); req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* r = net_->post(req, QByteArray("{}"));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        if (r->error() == QNetworkReply::NoError) {
            auto* toast = new QLabel("Tool installation queued!", this);
            toast->setStyleSheet("background:#22c55e;color:#080808;padding:10px 20px;font-weight:700;"
                                 "border-radius:4px;");
            toast->setAlignment(Qt::AlignCenter);
            toast->setWindowFlags(Qt::ToolTip);
            toast->adjustSize();
            toast->show();
            QTimer::singleShot(3000, toast, &QObject::deleteLater);
        }
        r->deleteLater();
    });
}

void CommunityMarketplaceScreen::install_agent(const QString& agentId) {
    QUrl url(base_url_ + "/agents/" + agentId + "/install");
    QNetworkRequest req(url); req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    net_->post(req, QByteArray("{}"));
}

} // namespace fincept::screens
