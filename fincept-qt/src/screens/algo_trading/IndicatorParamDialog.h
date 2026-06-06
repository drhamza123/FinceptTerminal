#pragma once
#include <QDialog>
#include <QString>

class QSpinBox;
class QDoubleSpinBox;

namespace fincept::screens {

class IndicatorParamDialog : public QDialog {
    Q_OBJECT
public:
    struct IndicatorParams {
        QString name;
        int period1 = 9;
        int period2 = 21;
        int period3 = 14;
        double multiplier = 2.0;
    };
    explicit IndicatorParamDialog(const QString& indicatorName, const IndicatorParams& params, QWidget* parent = nullptr);
    IndicatorParams result() const;

private:
    void buildUI(const QString& name);
    QSpinBox* period1_spin_ = nullptr;
    QSpinBox* period2_spin_ = nullptr;
    QSpinBox* period3_spin_ = nullptr;
    QDoubleSpinBox* mult_spin_ = nullptr;
    IndicatorParams params_;
};

} // namespace fincept::screens
