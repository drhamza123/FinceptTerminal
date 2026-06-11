#include "ui/charts/ChartScreenshot.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QWidget>

namespace fincept::ui {

QPixmap ChartScreenshot::capture(QWidget* chart_widget) {
    if (!chart_widget) return {};
    return chart_widget->grab();
}

bool ChartScreenshot::save_to_file(QWidget* chart_widget, const QString& filepath) {
    auto pix = capture(chart_widget);
    if (pix.isNull()) return false;
    return pix.save(filepath, "PNG");
}

bool ChartScreenshot::save_to_clipboard(QWidget* chart_widget) {
    auto pix = capture(chart_widget);
    if (pix.isNull()) return false;
    QApplication::clipboard()->setPixmap(pix);
    return true;
}

QString ChartScreenshot::suggested_filename(const QString& symbol, const QString& timeframe) {
    QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1_%2_%3.png").arg(symbol, timeframe, ts);
}

} // namespace fincept::ui
