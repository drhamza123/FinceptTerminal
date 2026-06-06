// MT5FleetMarketplacePanel.h — EA Marketplace
#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>

namespace fincept::screens {

class MT5FleetMarketplacePanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetMarketplacePanel(QWidget* parent = nullptr);
    ~MT5FleetMarketplacePanel() override;

  private slots:
    void refresh_marketplace();
    void on_install_clicked();
    void on_search_changed(const QString& text);
    void on_category_changed(int idx);

  private:
    void build_ui();
    void apply_theme();
    void update_marketplace_table(const QJsonArray& eas);

    // UI
    QLineEdit* search_edit_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QTableWidget* marketplace_table_ = nullptr;
    QPushButton* install_btn_ = nullptr;
    QLabel* status_label_ = nullptr;

    // State
    QString selected_ea_id_;
    QString search_text_;
};

} // namespace fincept::screens
