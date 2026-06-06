// MT5FleetSettingsTab.cpp
#include "screens/algo_trading/MT5FleetSettingsTab.h"
#include "ui/theme/Theme.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace fincept::screens {
MT5FleetSettingsTab::MT5FleetSettingsTab(QWidget* parent) : QWidget(parent) { build_ui(); }

void MT5FleetSettingsTab::build_ui() {
    auto* vl = new QVBoxLayout(this); vl->setContentsMargins(14,14,14,14); vl->setSpacing(12);
    auto* title = new QLabel("MT5 BRIDGE SETTINGS");
    title->setStyleSheet(QString("color:%1;font-size:14px;font-weight:700;").arg(ui::colors::AMBER()));
    vl->addWidget(title);

    auto* form = new QFormLayout; form->setSpacing(8);
    host_edit_ = new QLineEdit("127.0.0.1"); port_edit_ = new QLineEdit("5556");
    host_edit_->setStyleSheet(QString("QLineEdit{background:%1;color:%2;border:1px solid %3;padding:6px 10px;font-size:12px;}").arg(ui::colors::BG_BASE(),ui::colors::TEXT_PRIMARY(),ui::colors::BORDER_MED()));
    port_edit_->setStyleSheet(host_edit_->styleSheet());
    form->addRow("Bridge Host:", host_edit_);
    form->addRow("Bridge Port:", port_edit_);
    vl->addLayout(form);

    save_btn_ = new QPushButton("SAVE SETTINGS");
    save_btn_->setFixedHeight(32);
    save_btn_->setStyleSheet("QPushButton{background:#1a3a1a;color:#00D66F;border:1px solid #00D66F;font-size:11px;font-weight:700;}");
    connect(save_btn_, &QPushButton::clicked, this, [this](){
        QMessageBox::information(this,"Settings","Bridge settings saved.\n\nHost: "+host_edit_->text()+"\nPort: "+port_edit_->text());
    });
    vl->addWidget(save_btn_);

    status_label_ = new QLabel("Bridge status: checking...");
    status_label_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_SECONDARY()));
    vl->addWidget(status_label_);
    vl->addStretch();

    auto* info = new QLabel("The MT5 Bridge listens on localhost:5556 for EA connections.\n"
                            "Set MT5_BRIDGE_HOST and MT5_BRIDGE_PORT in .env for custom configuration.");
    info->setWordWrap(true);
    info->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    vl->addWidget(info);
}
} // namespace
