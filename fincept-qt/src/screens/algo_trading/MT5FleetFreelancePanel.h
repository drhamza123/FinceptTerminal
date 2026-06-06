// MT5FleetFreelancePanel.h — Freelance Marketplace for MQL5 Development
#pragma once
#include <QWidget>
class QTableWidget; class QComboBox; class QPushButton; class QLabel; class QLineEdit;

namespace fincept::screens {

class MT5FleetFreelancePanel : public QWidget {
    Q_OBJECT
  public:
    explicit MT5FleetFreelancePanel(QWidget* parent = nullptr);
    ~MT5FleetFreelancePanel() override;

  private slots:
    void refresh_projects();
    void refresh_developers();
    void on_search_changed(const QString& text);
    void on_category_changed(int idx);
    void on_project_clicked(int row, int col);

  private:
    void build_ui();
    void apply_theme();

    QLineEdit* search_input_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QTableWidget* projects_table_ = nullptr;
    QTableWidget* developers_table_ = nullptr;
    QLabel* status_label_ = nullptr;
};

} // namespace
