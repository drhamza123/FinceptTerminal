#include "screens/algo_trading/IndicatorParamDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>

namespace fincept::screens {

IndicatorParamDialog::IndicatorParamDialog(const QString& indicatorName, const IndicatorParams& params, QWidget* parent)
    : QDialog(parent)
    , params_(params)
{
    setWindowTitle("Indicator Parameters - " + indicatorName);
    setMinimumSize(320, 220);
    setObjectName("indicatorParamDialog");
    buildUI(indicatorName);
}

IndicatorParamDialog::IndicatorParams IndicatorParamDialog::result() const
{
    return params_;
}

void IndicatorParamDialog::buildUI(const QString& name)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto* title = new QLabel(name);
    title->setObjectName("indicatorDialogTitle");
    root->addWidget(title);

    auto* form = new QFormLayout();
    form->setSpacing(8);

    QString uc = name.toUpper();
    if (uc == "RSI") {
        period3_spin_ = new QSpinBox(this);
        period3_spin_->setRange(2, 100);
        period3_spin_->setValue(params_.period3);
        form->addRow("Period:", period3_spin_);
    } else if (uc == "MACD") {
        period1_spin_ = new QSpinBox(this);
        period1_spin_->setRange(2, 100);
        period1_spin_->setValue(params_.period1);
        form->addRow("Fast:", period1_spin_);

        period2_spin_ = new QSpinBox(this);
        period2_spin_->setRange(2, 200);
        period2_spin_->setValue(params_.period2);
        form->addRow("Slow:", period2_spin_);

        period3_spin_ = new QSpinBox(this);
        period3_spin_->setRange(2, 100);
        period3_spin_->setValue(params_.period3);
        form->addRow("Signal:", period3_spin_);
    } else if (uc == "BOLLINGER" || uc.contains("BOLL")) {
        period1_spin_ = new QSpinBox(this);
        period1_spin_->setRange(2, 100);
        period1_spin_->setValue(params_.period1);
        form->addRow("Period:", period1_spin_);

        mult_spin_ = new QDoubleSpinBox(this);
        mult_spin_->setRange(0.1, 10.0);
        mult_spin_->setSingleStep(0.1);
        mult_spin_->setValue(params_.multiplier);
        form->addRow("Std Dev:", mult_spin_);
    } else {
        period1_spin_ = new QSpinBox(this);
        period1_spin_->setRange(2, 200);
        period1_spin_->setValue(params_.period1);
        form->addRow("Period:", period1_spin_);
    }

    root->addLayout(form);
    root->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto* okBtn = new QPushButton("OK");
    okBtn->setObjectName("indicatorDialogBtn");
    connect(okBtn, &QPushButton::clicked, this, [this]() {
        if (period1_spin_) params_.period1 = period1_spin_->value();
        if (period2_spin_) params_.period2 = period2_spin_->value();
        if (period3_spin_) params_.period3 = period3_spin_->value();
        if (mult_spin_) params_.multiplier = mult_spin_->value();
        accept();
    });

    auto* cancelBtn = new QPushButton("Cancel");
    cancelBtn->setObjectName("indicatorDialogBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    root->addLayout(btnLayout);

    setStyleSheet(QString(
        "QDialog#indicatorParamDialog{background:%1;}"
        "QLabel#indicatorDialogTitle{color:%2;font-size:14px;font-weight:700;padding-bottom:4px;}"
        "QSpinBox,QDoubleSpinBox{background:%3;color:%2;border:1px solid %4;padding:4px 8px;font-size:11px;border-radius:3px;min-height:20px;}"
        "QPushButton#indicatorDialogBtn{background:%5;color:%1;border:none;padding:6px 20px;font-size:11px;font-weight:700;border-radius:3px;}"
        "QPushButton#indicatorDialogBtn:hover{background:%6;}"
        "QFormLayout{color:%2;font-size:11px;}"
    ).arg("#1a1a2e", "#e2b714", "#16213e", "#2a2a4a", "#e2b714", "#f0c830"));
}

} // namespace fincept::screens
