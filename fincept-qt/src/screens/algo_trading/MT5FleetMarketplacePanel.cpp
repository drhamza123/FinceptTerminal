// MT5FleetMarketplacePanel.cpp — EA Marketplace
#include "screens/algo_trading/MT5FleetMarketplacePanel.h"
#include "network/http/HttpClient.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QTimer>

namespace fincept::screens {

MT5FleetMarketplacePanel::MT5FleetMarketplacePanel(QWidget* parent) : QWidget(parent) {
    build_ui();
    apply_theme();
    refresh_marketplace();
}

MT5FleetMarketplacePanel::~MT5FleetMarketplacePanel() = default;

void MT5FleetMarketplacePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header with search and category filter
    auto* header = new QWidget(this);
    header->setObjectName("marketplaceHeader");
    header->setFixedHeight(50);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(12, 8, 12, 8);
    h_layout->setSpacing(8);

    auto* title_label = new QLabel("MARKETPLACE", header);
    title_label->setObjectName("marketplaceTitleLabel");
    h_layout->addWidget(title_label);

    h_layout->addStretch();

    search_edit_ = new QLineEdit(header);
    search_edit_->setObjectName("marketplaceSearchEdit");
    search_edit_->setPlaceholderText("Search EAs...");
    search_edit_->setFixedWidth(200);
    connect(search_edit_, &QLineEdit::textChanged, this, &MT5FleetMarketplacePanel::on_search_changed);
    h_layout->addWidget(search_edit_);

    category_combo_ = new QComboBox(header);
    category_combo_->setObjectName("marketplaceCategoryCombo");
    category_combo_->addItems({"All Categories", "Trend Following", "Scalping", "Grid", "Martingale", "News Trading", "Custom"});
    connect(category_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MT5FleetMarketplacePanel::on_category_changed);
    h_layout->addWidget(category_combo_);

    root->addWidget(header);

    // Marketplace table
    marketplace_table_ = new QTableWidget(0, 7, this);
    marketplace_table_->setObjectName("marketplaceTable");
    marketplace_table_->setHorizontalHeaderLabels({
        "EA Name", "Author", "Category", "Rating", "Downloads", "Price", "Status"
    });
    marketplace_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    marketplace_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    marketplace_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    marketplace_table_->verticalHeader()->setVisible(false);
    root->addWidget(marketplace_table_, 1);

    // Bottom bar
    auto* bottom = new QWidget(this);
    bottom->setObjectName("marketplaceBottom");
    bottom->setFixedHeight(50);
    auto* b_layout = new QHBoxLayout(bottom);
    b_layout->setContentsMargins(12, 8, 12, 8);
    b_layout->setSpacing(12);

    status_label_ = new QLabel("Select an EA to install", bottom);
    status_label_->setObjectName("marketplaceStatusLabel");
    b_layout->addWidget(status_label_, 1);

    install_btn_ = new QPushButton("INSTALL EA", bottom);
    install_btn_->setObjectName("marketplaceInstallBtn");
    install_btn_->setFixedHeight(34);
    install_btn_->setEnabled(false);
    connect(install_btn_, &QPushButton::clicked, this, &MT5FleetMarketplacePanel::on_install_clicked);
    b_layout->addWidget(install_btn_);

    root->addWidget(bottom);

    // Connect table selection
    connect(marketplace_table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        auto selected = marketplace_table_->selectedItems();
        install_btn_->setEnabled(!selected.isEmpty());
        if (!selected.isEmpty()) {
            auto* name_item = marketplace_table_->item(selected.first()->row(), 0);
            if (name_item) {
                selected_ea_id_ = name_item->text();
                status_label_->setText(QString("Selected: %1").arg(selected_ea_id_));
            }
        }
    });
}

void MT5FleetMarketplacePanel::apply_theme() {
    setStyleSheet(QString(
        "QWidget#marketplaceHeader{background:%1;border-bottom:1px solid %2;}"
        "QLabel#marketplaceTitleLabel{color:%3;font-size:14px;font-weight:700;}"
        "QLineEdit#marketplaceSearchEdit{background:%4;color:%3;border:1px solid %2;padding:4px 8px;}"
        "QComboBox#marketplaceCategoryCombo{background:%4;color:%3;border:1px solid %2;padding:4px 8px;min-width:120px;}"
        "QTableWidget#marketplaceTable{background:%5;color:%3;border:1px solid %2;}"
        "QTableWidget::item{padding:4px 8px;font-size:11px;}"
        "QWidget#marketplaceBottom{background:%1;border-top:1px solid %2;}"
        "QLabel#marketplaceStatusLabel{color:%6;font-size:11px;}"
        "QPushButton#marketplaceInstallBtn{background:%7;color:#FFF;border:none;font-size:12px;font-weight:700;padding:0 20px;}"
        "QPushButton#marketplaceInstallBtn:hover{background:#00B85C;}"
        "QPushButton#marketplaceInstallBtn:disabled{background:%8;color:%6;}"
    ).arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), ui::colors::TEXT_PRIMARY(),
          ui::colors::BG_RAISED(), ui::colors::BG_BASE(), ui::colors::TEXT_TERTIARY(),
          ui::colors::POSITIVE(), ui::colors::BG_HOVER()));
}

void MT5FleetMarketplacePanel::refresh_marketplace() {
    QString endpoint = "/mt5/marketplace";
    
    if (!search_text_.isEmpty()) {
        endpoint += QString("?search=%1").arg(search_text_);
    }
    
    QString category = category_combo_->currentText();
    if (category != "All Categories") {
        endpoint += QString("%1category=%2").arg(search_text_.isEmpty() ? "?" : "&", category);
    }

    HttpClient::instance().get(endpoint, [this](Result<QJsonDocument> result) {
        if (result.is_err()) return;
        auto doc = result.value();
        auto eas = doc.object()["eas"].toArray();
        update_marketplace_table(eas);
    }, this);
}

void MT5FleetMarketplacePanel::update_marketplace_table(const QJsonArray& eas) {
    marketplace_table_->setRowCount(eas.size());
    
    for (int i = 0; i < eas.size(); ++i) {
        auto ea = eas[i].toObject();
        
        marketplace_table_->setItem(i, 0, new QTableWidgetItem(ea["name"].toString()));
        marketplace_table_->setItem(i, 1, new QTableWidgetItem(ea["author"].toString()));
        marketplace_table_->setItem(i, 2, new QTableWidgetItem(ea["category"].toString()));
        
        auto* rating_item = new QTableWidgetItem(QString("%1★").arg(ea["rating"].toDouble(), 0, 'f', 1));
        rating_item->setForeground(QColor("#FFC400"));
        marketplace_table_->setItem(i, 3, rating_item);
        
        marketplace_table_->setItem(i, 4, new QTableWidgetItem(QString::number(ea["downloads"].toInt())));
        
        double price = ea["price"].toDouble();
        auto* price_item = new QTableWidgetItem(price > 0 ? QString("$%1").arg(price, 0, 'f', 2) : "Free");
        if (price == 0) price_item->setForeground(QColor(ui::colors::POSITIVE()));
        marketplace_table_->setItem(i, 5, price_item);
        
        auto* status_item = new QTableWidgetItem(ea["status"].toString());
        if (ea["status"].toString() == "Available") {
            status_item->setForeground(QColor(ui::colors::POSITIVE()));
        } else if (ea["status"].toString() == "Installed") {
            status_item->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        }
        marketplace_table_->setItem(i, 6, status_item);
    }
}

void MT5FleetMarketplacePanel::on_install_clicked() {
    if (selected_ea_id_.isEmpty()) return;
    
    auto reply = QMessageBox::question(
        this, "Install EA",
        QString("Install %1 to your MT5 terminal?\n\nThis will download and compile the EA.").arg(selected_ea_id_),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        HttpClient::instance().post(
            QString("/mt5/marketplace/install"),
            QJsonObject{{"ea_id", selected_ea_id_}},
            [this](Result<QJsonDocument> result) {
                if (result.is_err()) {
                    QMessageBox::warning(this, "Error", "Failed to install EA");
                    return;
                }
                QMessageBox::information(this, "Success", "EA installed successfully!");
                status_label_->setText("EA installed!");
                refresh_marketplace();
            }, this);
    }
}

void MT5FleetMarketplacePanel::on_search_changed(const QString& text) {
    search_text_ = text;
    refresh_marketplace();
}

void MT5FleetMarketplacePanel::on_category_changed(int idx) {
    refresh_marketplace();
}

} // namespace fincept::screens
