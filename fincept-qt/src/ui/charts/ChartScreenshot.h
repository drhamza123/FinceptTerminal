#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>

class QWidget;

namespace fincept::ui {

class ChartScreenshot {
public:
    static QPixmap capture(QWidget* chart_widget);
    static bool save_to_file(QWidget* chart_widget, const QString& filepath);
    static bool save_to_clipboard(QWidget* chart_widget);
    static QString suggested_filename(const QString& symbol, const QString& timeframe);
};

} // namespace fincept::ui
