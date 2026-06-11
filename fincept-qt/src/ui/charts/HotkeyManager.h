#pragma once

#include <QObject>
#include <QKeyEvent>
#include <QHash>

class QWidget;

namespace fincept::ui {

class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);

    void register_shortcut(const QKeySequence& keys, const QString& action);
    bool handle_key(QKeyEvent* event);

signals:
    void fullscreen_toggled();
    void undo();
    void redo();
    void screenshot();
    void toggle_drawing();
    void toggle_crosshair();
    void zoom_in();
    void zoom_out();
    void scroll_left();
    void scroll_right();
    void indicator_requested(const QString& id);

private:
    struct HotkeyEntry {
        QKeySequence keys;
        QString action;
    };
    QVector<HotkeyEntry> bindings_;
};

} // namespace fincept::ui
