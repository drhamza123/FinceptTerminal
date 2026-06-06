// MT5FleetEcoCalendarPanel.h — Economic Calendar with impact/forecast/actual
#pragma once
#include <QWidget>
#include <QTimer>
#include <QVector>

class QTableWidget;
class QComboBox;
class QLabel;
class QCheckBox;

namespace fincept::screens {

struct EcoEvent {
    QString event_id;
    QString title;
    QString date;
    QString time;
    QString country;
    QString category;
    QString importance; // "high", "medium", "low"
    QString previous;
    QString forecast;
    QString actual;
};

class MT5FleetEcoCalendarPanel : public QWidget {
    Q_OBJECT
public:
    explicit MT5FleetEcoCalendarPanel(QWidget* parent = nullptr);
    ~MT5FleetEcoCalendarPanel() override;

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private slots:
    void refresh_events();
    void on_filter_change();

private:
    void build_ui();
    void apply_theme();
    void populate_table(const QVector<EcoEvent>& events);
    void add_event_row(int row, const EcoEvent& event);
    QColor imp_country_color(const QString& country) const;
    QColor imp_color(const QString& importance) const;
    QString country_flag(const QString& country) const;

    QTableWidget* table_ = nullptr;
    QComboBox* impact_filter_ = nullptr;
    QComboBox* country_filter_ = nullptr;
    QComboBox* category_filter_ = nullptr;
    QLabel* count_label_ = nullptr;
    QTimer* timer_ = nullptr;

    QVector<EcoEvent> all_events_;
    bool is_active_ = false;
};

} // namespace fincept::screens
