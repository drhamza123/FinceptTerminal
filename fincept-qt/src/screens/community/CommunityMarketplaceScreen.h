#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTabWidget>
#include <QMessageBox>

namespace fincept::screens {

class CommunityMarketplaceScreen : public QWidget {
    Q_OBJECT
  public:
    explicit CommunityMarketplaceScreen(QWidget* parent = nullptr);

  private slots:
    void on_search();
    void on_category_changed(int);
    void on_tools_fetched();
    void on_agents_fetched();
    void install_tool(const QString& toolId);
    void install_agent(const QString& agentId);

  private:
    void build_ui();
    void fetch_tools();
    void fetch_agents();
    QWidget* build_tool_row(const QJsonObject& tool);

    QLineEdit* search_edit_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QWidget* tools_tab_ = nullptr;
    QWidget* agents_tab_ = nullptr;
    QVBoxLayout* tools_layout_ = nullptr;
    QVBoxLayout* agents_layout_ = nullptr;
    QNetworkAccessManager* net_ = nullptr;
    QString base_url_ = "http://localhost:8150/community";
};

} // namespace fincept::screens
