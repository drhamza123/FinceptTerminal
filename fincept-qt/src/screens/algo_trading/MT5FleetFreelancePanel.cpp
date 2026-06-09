// MT5FleetFreelancePanel.cpp — Freelance Marketplace for MQL5 Development
#include "screens/algo_trading/MT5FleetFreelancePanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTabWidget>

namespace fincept::screens {

MT5FleetFreelancePanel::MT5FleetFreelancePanel(QWidget* parent) : QWidget(parent) {
    build_ui(); apply_theme();
    refresh_projects();
}

MT5FleetFreelancePanel::~MT5FleetFreelancePanel() = default;

void MT5FleetFreelancePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* head = new QWidget(this);
    head->setObjectName("freelanceHeader");
    head->setFixedHeight(40);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* title = new QLabel("FREELANCE MARKETPLACE", head);
    title->setObjectName("freelanceTitle");
    hl->addWidget(title);
    hl->addStretch();

    search_input_ = new QLineEdit(head);
    search_input_->setObjectName("freelanceSearch");
    search_input_->setPlaceholderText("Search projects...");
    search_input_->setFixedWidth(200);
    connect(search_input_, &QLineEdit::textChanged, this, &MT5FleetFreelancePanel::on_search_changed);
    hl->addWidget(search_input_);

    category_combo_ = new QComboBox(head);
    category_combo_->setObjectName("freelanceCombo");
    category_combo_->addItems({"All", "Expert Advisor", "Indicator", "Utility", "Script", "Dashboard"});
    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MT5FleetFreelancePanel::on_category_changed);
    hl->addWidget(category_combo_);

    refresh_btn_ = new QPushButton("Refresh", head);
    refresh_btn_->setObjectName("freelanceButton");
    connect(refresh_btn_, &QPushButton::clicked, this, &MT5FleetFreelancePanel::refresh_projects);
    hl->addWidget(refresh_btn_);

    root->addWidget(head);

    // Tabs
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("freelanceTabs");

    // Projects tab
    auto* proj_tab = new QWidget();
    auto* proj_layout = new QVBoxLayout(proj_tab);
    proj_layout->setContentsMargins(8, 8, 8, 8);

    projects_table_ = new QTableWidget(0, 7, proj_tab);
    projects_table_->setObjectName("freelanceTable");
    projects_table_->setHorizontalHeaderLabels({"Title", "Category", "Budget", "Skills", "Posted By", "Bids", "Status"});
    projects_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    projects_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    projects_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    projects_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    projects_table_->verticalHeader()->setVisible(false);
    connect(projects_table_, &QTableWidget::cellClicked, this, &MT5FleetFreelancePanel::on_project_clicked);
    proj_layout->addWidget(projects_table_, 1);

    status_label_ = new QLabel("Click a project to view details", proj_tab);
    status_label_->setObjectName("freelanceStatus");
    proj_layout->addWidget(status_label_);

    tabs->addTab(proj_tab, "Projects");

    // Developers tab
    auto* dev_tab = new QWidget();
    auto* dev_layout = new QVBoxLayout(dev_tab);
    dev_layout->setContentsMargins(8, 8, 8, 8);

    developers_table_ = new QTableWidget(0, 6, dev_tab);
    developers_table_->setObjectName("freelanceTable");
    developers_table_->setHorizontalHeaderLabels({"Name", "Rating", "Jobs", "Skills", "Rate", "Availability"});
    developers_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    developers_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    developers_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    developers_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    developers_table_->verticalHeader()->setVisible(false);
    dev_layout->addWidget(developers_table_, 1);

    tabs->addTab(dev_tab, "Developers");
    root->addWidget(tabs, 1);
}

void MT5FleetFreelancePanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#freelanceHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#freelanceTitle{color:%3;font-size:13px;font-weight:700;}"
        "QLineEdit#freelanceSearch{background:%4;color:%3;border:1px solid %2;padding:4px 8px;}"
        "QComboBox#freelanceCombo{background:%4;color:%3;border:1px solid %2;padding:4px 8px;}"
        "QPushButton#freelanceButton{background:%4;color:%3;border:1px solid %2;padding:4px 12px;}"
        "QPushButton#freelanceButton:hover{background:%5;}"
        "QTabWidget#freelanceTabs::pane{background:%6;border:1px solid %2;}"
        "QTabBar::tab{background:%4;color:%7;padding:6px 16px;border:1px solid %2;}"
        "QTabBar::tab:selected{background:%1;color:%3;border-bottom:1px solid %1;}"
        "QTableWidget#freelanceTable{background:%6;color:%3;gridline-color:%2;border:1px solid %2;}"
        "QHeaderView::section{background:%1;color:%3;padding:4px 8px;font-weight:700;font-size:10px;border:none;border-bottom:1px solid %2;}"
        "QLabel#freelanceStatus{color:%8;font-size:11px;padding:6px;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_RAISED(), ui::colors::BG_HOVER(), ui::colors::BG_BASE(),
          ui::colors::TEXT_TERTIARY()));
}

void MT5FleetFreelancePanel::refresh_projects() {
    QString url = "/freelance/projects";
    QString cat = category_combo_->currentText();
    if (cat != "All") url += "?category=" + cat;
    QString search = search_input_->text().trimmed();
    if (!search.isEmpty()) url += (cat == "All" ? "?" : "&") + QString("search=") + search;

    HttpClient::instance().get(url, [this](Result<QJsonDocument> r) {
        if (r.is_err()) return;
        auto arr = r.value().object()["data"].toArray();
        projects_table_->setRowCount(arr.size());
        for (int i = 0; i < arr.size(); ++i) {
            auto obj = arr[i].toObject();
            projects_table_->setItem(i, 0, new QTableWidgetItem(obj["title"].toString()));
            projects_table_->setItem(i, 1, new QTableWidgetItem(obj["category"].toString()));
            projects_table_->setItem(i, 2, new QTableWidgetItem(obj["budget"].toString()));
            QStringList js; for (auto v : obj["skills"].toArray()) js << v.toString();
            projects_table_->setItem(i, 3, new QTableWidgetItem(js.join(", ")));
            projects_table_->setItem(i, 4, new QTableWidgetItem(obj["posted_by"].toString()));
            projects_table_->setItem(i, 5, new QTableWidgetItem(QString::number(obj["bids"].toInt())));
            projects_table_->setItem(i, 6, new QTableWidgetItem(obj["status"].toString()));
        }
        status_label_->setText(QString("%1 projects found").arg(arr.size()));
    }, this);
}

void MT5FleetFreelancePanel::refresh_developers() {
    HttpClient::instance().get("/freelance/developers", [this](Result<QJsonDocument> r) {
        if (r.is_err()) return;
        auto arr = r.value().object()["data"].toArray();
        developers_table_->setRowCount(arr.size());
        for (int i = 0; i < arr.size(); ++i) {
            auto obj = arr[i].toObject();
            developers_table_->setItem(i, 0, new QTableWidgetItem(obj["name"].toString()));
            auto* rating = new QTableWidgetItem(QString::number(obj["rating"].toDouble(), 'f', 1));
            rating->setForeground(QColor(ui::colors::POSITIVE()));
            developers_table_->setItem(i, 1, rating);
            developers_table_->setItem(i, 2, new QTableWidgetItem(QString::number(obj["completed_jobs"].toInt())));
            QStringList sl; for (auto v : obj["skills"].toArray()) sl << v.toString();
            developers_table_->setItem(i, 3, new QTableWidgetItem(sl.join(", ")));
            developers_table_->setItem(i, 4, new QTableWidgetItem(obj["hourly_rate"].toString()));
            auto* avail = new QTableWidgetItem(obj["availability"].toString());
            if (obj["availability"].toString() == "Full-time")
                avail->setForeground(QColor(ui::colors::POSITIVE()));
            developers_table_->setItem(i, 5, avail);
        }
    }, this);
}

void MT5FleetFreelancePanel::on_search_changed(const QString&) {
    refresh_projects();
}

void MT5FleetFreelancePanel::on_category_changed(int) {
    refresh_projects();
}

void MT5FleetFreelancePanel::on_project_clicked(int row, int) {
    auto* item = projects_table_->item(row, 0);
    if (item) {
        status_label_->setText(QString("Selected: %1 | Budget: %2 | Bids: %3")
            .arg(item->text(),
                 projects_table_->item(row, 2)->text(),
                 projects_table_->item(row, 5)->text()));
    }
}

} // namespace fincept::screens
