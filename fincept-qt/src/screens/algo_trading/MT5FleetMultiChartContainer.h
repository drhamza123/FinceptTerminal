// MT5FleetMultiChartContainer.h — Multi-chart tiling (2x2, 3x1, 1x3)
#pragma once
#include <QWidget>
#include <QVector>

class QPushButton;
class QComboBox;
class QGridLayout;

namespace fincept::screens {

class MT5FleetChartTile;

enum class TileLayout { Single, TwoByTwo, ThreeHorizontal, ThreeVertical, FourSquare };

class MT5FleetMultiChartContainer : public QWidget {
    Q_OBJECT
public:
    explicit MT5FleetMultiChartContainer(QWidget* parent = nullptr);
    ~MT5FleetMultiChartContainer() override;

    void setLayoutType(TileLayout layout);
    TileLayout layoutType() const { return current_layout_; }

signals:
    void symbolSelected(const QString& symbol);

private slots:
    void on_layout_changed(int idx);
    void on_tile_symbol_changed();

private:
    void build_ui();
    void apply_theme();
    void rebuild_tiles();
    void clear_tiles();
    MT5FleetChartTile* create_tile(int index);
    void sync_tiles();

    TileLayout current_layout_ = TileLayout::TwoByTwo;
    QVector<MT5FleetChartTile*> tiles_;
    QGridLayout* tile_grid_ = nullptr;
    QComboBox* layout_combo_ = nullptr;
    QComboBox* timeframe_combo_ = nullptr;
    QPushButton* sync_all_btn_ = nullptr;

    QStringList default_symbols_;
    QString current_timeframe_ = "H1";
    bool sync_enabled_ = true;
};

} // namespace fincept::screens
