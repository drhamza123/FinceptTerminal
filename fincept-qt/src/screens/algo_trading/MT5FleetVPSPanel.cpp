// MT5FleetVPSPanel.cpp — VPS Management for MT5 EA Hosting
#include "screens/algo_trading/MT5FleetVPSPanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QInputDialog>

namespace fincept::screens {

MT5FleetVPSPanel::MT5FleetVPSPanel(QWidget* parent) : QWidget(parent) {
    build_ui(); apply_theme();
    refresh_plans();
    refresh_instances();
}

MT5FleetVPSPanel::~MT5FleetVPSPanel() = default;

void MT5FleetVPSPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto* head = new QWidget(this);
    head->setObjectName("vpsHeader");
    head->setFixedHeight(40);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);

    auto* title = new QLabel("VPS MANAGEMENT", head);
    title->setObjectName("vpsTitle");
    hl->addWidget(title);
    hl->addStretch();

    plan_combo_ = new QComboBox(head);
    plan_combo_->setObjectName("vpsCombo");
    hl->addWidget(plan_combo_);

    deploy_btn_ = new QPushButton("Deploy EA", head);
    deploy_btn_->setObjectName("vpsButton");
    connect(deploy_btn_, &QPushButton::clicked, this, &MT5FleetVPSPanel::on_deploy);
    hl->addWidget(deploy_btn_);

    refresh_btn_ = new QPushButton("Refresh", head);
    refresh_btn_->setObjectName("vpsButton");
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() { refresh_plans(); refresh_instances(); });
    hl->addWidget(refresh_btn_);

    root->addWidget(head);

    // Plans section
    auto* plans_label = new QLabel("AVAILABLE PLANS", this);
    plans_label->setObjectName("vpsSectionLabel");
    plans_label->setFixedHeight(28);
    root->addWidget(plans_label);

    plans_table_ = new QTableWidget(0, 6, this);
    plans_table_->setObjectName("vpsTable");
    plans_table_->setHorizontalHeaderLabels({"Plan", "RAM", "CPU", "Storage", "Bandwidth", "Price"});
    plans_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    plans_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    plans_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    plans_table_->verticalHeader()->setVisible(false);
    plans_table_->setMaximumHeight(180);
    root->addWidget(plans_table_);

    // Instances section
    auto* inst_label = new QLabel("YOUR INSTANCES", this);
    inst_label->setObjectName("vpsSectionLabel");
    inst_label->setFixedHeight(28);
    root->addWidget(inst_label);

    instances_table_ = new QTableWidget(0, 6, this);
    instances_table_->setObjectName("vpsTable");
    instances_table_->setHorizontalHeaderLabels({"Name", "Plan", "IP", "Status", "EA", "Actions"});
    instances_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    instances_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    instances_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    instances_table_->verticalHeader()->setVisible(false);
    root->addWidget(instances_table_, 1);

    status_label_ = new QLabel("Select a plan and deploy your EA", this);
    status_label_->setObjectName("vpsStatus");
    root->addWidget(status_label_);
}

void MT5FleetVPSPanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#vpsHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#vpsTitle{color:%3;font-size:13px;font-weight:700;}"
        "QComboBox#vpsCombo{background:%4;color:%3;border:1px solid %2;padding:4px 8px;}"
        "QPushButton#vpsButton{background:%4;color:%3;border:1px solid %2;padding:4px 12px;}"
        "QPushButton#vpsButton:hover{background:%5;}"
        "QLabel#vpsSectionLabel{background:%1;color:%6;font-size:10px;font-weight:700;letter-spacing:1.2px;padding:0 12px;border-bottom:1px solid %2;}"
        "QTableWidget#vpsTable{background:%7;color:%3;gridline-color:%2;border:1px solid %2;}"
        "QHeaderView::section{background:%1;color:%3;padding:4px 8px;font-weight:700;font-size:10px;border:none;border-bottom:1px solid %2;}"
        "QLabel#vpsStatus{color:%8;font-size:11px;padding:6px;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_RAISED(), ui::colors::BG_HOVER(), ui::colors::AMBER(),
          ui::colors::BG_BASE(), ui::colors::TEXT_TERTIARY()));
}

void MT5FleetVPSPanel::refresh_plans() {
    HttpClient::instance().get("/vps/plans", [this](Result<QJsonDocument> r) {
        if (r.is_err()) return;
        auto arr = r.value().object()["data"].toArray();
        plan_combo_->clear();
        plans_table_->setRowCount(arr.size());
        for (int i = 0; i < arr.size(); ++i) {
            auto obj = arr[i].toObject();
            plans_table_->setItem(i, 0, new QTableWidgetItem(obj["name"].toString()));
            plans_table_->setItem(i, 1, new QTableWidgetItem(obj["ram"].toString()));
            plans_table_->setItem(i, 2, new QTableWidgetItem(obj["cpu"].toString()));
            plans_table_->setItem(i, 3, new QTableWidgetItem(obj["storage"].toString()));
            plans_table_->setItem(i, 4, new QTableWidgetItem(obj["bandwidth"].toString()));
            plans_table_->setItem(i, 5, new QTableWidgetItem(obj["price"].toString()));
            plan_combo_->addItem(obj["name"].toString());
        }
    }, this);
}

void MT5FleetVPSPanel::refresh_instances() {
    HttpClient::instance().get("/vps/status", [this](Result<QJsonDocument> r) {
        if (r.is_err()) return;
        auto arr = r.value().object()["instances"].toArray();
        instances_table_->setRowCount(arr.size());
        for (int i = 0; i < arr.size(); ++i) {
            auto obj = arr[i].toObject();
            instances_table_->setItem(i, 0, new QTableWidgetItem(obj["name"].toString()));
            instances_table_->setItem(i, 1, new QTableWidgetItem(obj["plan"].toString()));
            instances_table_->setItem(i, 2, new QTableWidgetItem(obj["ip"].toString()));

            auto* status = new QTableWidgetItem(obj["status"].toString());
            if (obj["status"].toString() == "running")
                status->setForeground(QColor(ui::colors::POSITIVE()));
            else if (obj["status"].toString() == "stopped")
                status->setForeground(QColor(ui::colors::NEGATIVE()));
            instances_table_->setItem(i, 3, status);

            instances_table_->setItem(i, 4, new QTableWidgetItem(obj["ea_deployed"].toString()));

            auto* actions = new QTableWidgetItem("Start | Stop | Restart");
            actions->setForeground(QColor(ui::colors::AMBER()));
            instances_table_->setItem(i, 5, actions);
        }
        status_label_->setText(QString("%1 instance(s) running").arg(arr.size()));
    }, this);
}

void MT5FleetVPSPanel::on_deploy() {
    if (plan_combo_->currentText().isEmpty()) return;

    QString plan = plan_combo_->currentText();
    bool ok;
    QString ea_name = QInputDialog::getText(this, "Deploy EA",
        QString("Deploy EA to %1 plan.\nEA name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || ea_name.isEmpty()) return;

    QJsonObject payload;
    payload["plan"] = plan;
    payload["ea_name"] = ea_name;

    HttpClient::instance().post("/vps/deploy", payload,
        [this, ea_name](Result<QJsonDocument> r) {
            if (r.is_err()) {
                status_label_->setText("Deploy failed.");
                return;
            }
            auto obj = r.value().object();
            status_label_->setText(QString("Deployed: %1 — %2").arg(ea_name, obj["message"].toString()));
            refresh_instances();
        }, this);
}

void MT5FleetVPSPanel::on_action(const QString& action) {
    status_label_->setText(QString("Action: %1").arg(action));
}

} // namespace fincept::screens
