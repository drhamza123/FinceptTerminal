// MT5FleetMultiChartContainer.cpp — Multi-chart tiling container
#include "screens/algo_trading/MT5FleetMultiChartContainer.h"
#include "screens/algo_trading/MT5FleetChartTile.h"
#include "ui/theme/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

namespace fincept::screens {

MT5FleetMultiChartContainer::MT5FleetMultiChartContainer(QWidget* parent) : QWidget(parent) {
    default_symbols_ = {"XAUUSD","EURUSD","BTCUSD","AAPL"};
    build_ui();
    apply_theme();
    rebuild_tiles();
}

MT5FleetMultiChartContainer::~MT5FleetMultiChartContainer() = default;

void MT5FleetMultiChartContainer::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("multiChartHeader");
    header->setFixedHeight(36);
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10,0,10,0);
    hl->setSpacing(8);

    auto* title = new QLabel("PRO CHARTS", header);
    title->setObjectName("multiChartTitle");
    hl->addWidget(title);
    hl->addStretch();

    auto* layout_lbl = new QLabel("Layout:", header);
    layout_lbl->setObjectName("multiChartLabel");
    hl->addWidget(layout_lbl);

    layout_combo_ = new QComboBox(header);
    layout_combo_->setObjectName("multiChartCombo");
    layout_combo_->addItems({"1×1 Single","2×2 Quad","3×1 Horizontal","1×3 Vertical","2×2+2"});
    connect(layout_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MT5FleetMultiChartContainer::on_layout_changed);
    hl->addWidget(layout_combo_);

    auto* tf_lbl = new QLabel("TF:", header);
    tf_lbl->setObjectName("multiChartLabel");
    hl->addWidget(tf_lbl);

    timeframe_combo_ = new QComboBox(header);
    timeframe_combo_->setObjectName("multiChartCombo");
    timeframe_combo_->addItems({"M1","M5","M15","M30","H1","H4","D1","W1","MN1"});
    timeframe_combo_->setCurrentText("H1");
    connect(timeframe_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        current_timeframe_ = timeframe_combo_->currentText();
        if (sync_enabled_) sync_tiles();
    });
    hl->addWidget(timeframe_combo_);

    sync_all_btn_ = new QPushButton("🔗 Sync", header);
    sync_all_btn_->setObjectName("multiChartSyncBtn");
    sync_all_btn_->setCheckable(true);
    sync_all_btn_->setChecked(true);
    connect(sync_all_btn_, &QPushButton::toggled, this, [this](bool checked) {
        sync_enabled_ = checked;
    });
    hl->addWidget(sync_all_btn_);

    root->addWidget(header);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("multiChartSep");
    sep->setFixedHeight(1);
    root->addWidget(sep);

    // Tile grid
    auto* grid_container = new QWidget(this);
    tile_grid_ = new QGridLayout(grid_container);
    tile_grid_->setContentsMargins(4,4,4,4);
    tile_grid_->setSpacing(4);
    root->addWidget(grid_container, 1);
}

void MT5FleetMultiChartContainer::apply_theme() {
    setStyleSheet(QString(
        "QWidget#multiChartHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#multiChartTitle{color:%3;font-size:11px;font-weight:700;letter-spacing:1.5px;background:transparent;}"
        "QLabel#multiChartLabel{color:%4;font-size:10px;font-weight:600;background:transparent;}"
        "QComboBox#multiChartCombo{background:%5;color:%6;border:1px solid %2;padding:2px 6px;font-size:10px;}"
        "QPushButton#multiChartSyncBtn{background:%5;color:%6;border:1px solid %2;padding:2px 8px;font-size:10px;}"
        "QPushButton#multiChartSyncBtn:checked{background:%3;color:%7;}"
        "QFrame#multiChartSep{color:%2;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
          ui::colors::TEXT_TERTIARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_BASE()));
}

void MT5FleetMultiChartContainer::on_layout_changed(int idx) {
    setLayoutType(static_cast<TileLayout>(idx));
}

void MT5FleetMultiChartContainer::setLayoutType(TileLayout layout) {
    current_layout_ = layout;
    rebuild_tiles();
}

void MT5FleetMultiChartContainer::rebuild_tiles() {
    clear_tiles();

    int tile_count = 0;
    switch (current_layout_) {
        case TileLayout::Single: tile_count = 1; break;
        case TileLayout::TwoByTwo: tile_count = 4; break;
        case TileLayout::ThreeHorizontal: tile_count = 3; break;
        case TileLayout::ThreeVertical: tile_count = 3; break;
        case TileLayout::FourSquare: tile_count = 6; break;
    }

    for (int i = 0; i < tile_count; ++i) {
        auto* tile = create_tile(i);
        tiles_.append(tile);
    }

    // Arrange in grid
    int cols = tile_count;
    switch (current_layout_) {
        case TileLayout::Single: cols = 1; break;
        case TileLayout::TwoByTwo: cols = 2; break;
        case TileLayout::ThreeHorizontal: cols = 3; break;
        case TileLayout::ThreeVertical: cols = 1; break;
        case TileLayout::FourSquare: cols = 2; break;
    }

    for (int i = 0; i < tiles_.size(); ++i) {
        int r = i / cols;
        int c = i % cols;
        tile_grid_->addWidget(tiles_[i], r, c);
        // Set stretch factors
        tile_grid_->setRowStretch(r, 1);
        tile_grid_->setColumnStretch(c, 1);
    }
}

void MT5FleetMultiChartContainer::clear_tiles() {
    for (auto* tile : tiles_) {
        tile_grid_->removeWidget(tile);
        tile->deleteLater();
    }
    tiles_.clear();
}

MT5FleetChartTile* MT5FleetMultiChartContainer::create_tile(int index) {
    auto* tile = new MT5FleetChartTile(index, this);

    QString symbol = (index < default_symbols_.size()) ? default_symbols_[index]
                     : QString("SYM%1").arg(index + 1);
    tile->setSymbol(symbol);
    tile->setTimeframe(current_timeframe_);

    connect(tile, &MT5FleetChartTile::symbolChanged, this, &MT5FleetMultiChartContainer::on_tile_symbol_changed);

    return tile;
}

void MT5FleetMultiChartContainer::on_tile_symbol_changed() {
    // Forward symbol selection
    auto* tile = qobject_cast<MT5FleetChartTile*>(sender());
    if (tile) {
        emit symbolSelected(tile->symbol());
    }
}

void MT5FleetMultiChartContainer::sync_tiles() {
    for (auto* tile : tiles_) {
        tile->setTimeframe(current_timeframe_);
    }
}

} // namespace fincept::screens
