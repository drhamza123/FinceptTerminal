#pragma once
#include <QWidget>
#include <QVector>

namespace fincept::screens {

class VolumeProfileLayer : public QWidget {
    Q_OBJECT
public:
    explicit VolumeProfileLayer(QWidget* parent = nullptr);
    void setData(const QVector<qreal>& prices, const QVector<qreal>& volumes, int numBins = 24);
    void clear();
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    struct PriceLevel { qreal price; qreal volume; };
    QVector<PriceLevel> levels_;
    qreal maxVolume_ = 0;
};

} // namespace fincept::screens
