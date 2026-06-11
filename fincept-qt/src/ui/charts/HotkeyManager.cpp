#include "ui/charts/HotkeyManager.h"

namespace fincept::ui {

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    register_shortcut(QKeySequence("Alt+F"), "fullscreen");
    register_shortcut(QKeySequence("Ctrl+Z"), "undo");
    register_shortcut(QKeySequence("Ctrl+Shift+Z"), "redo");
    register_shortcut(QKeySequence("Ctrl+S"), "screenshot");
    register_shortcut(QKeySequence("Ctrl+D"), "toggle_drawing");
    register_shortcut(QKeySequence("Ctrl+H"), "toggle_crosshair");
    register_shortcut(QKeySequence("+"), "zoom_in");
    register_shortcut(QKeySequence("-"), "zoom_out");
    register_shortcut(QKeySequence("Left"), "scroll_left");
    register_shortcut(QKeySequence("Right"), "scroll_right");
}

void HotkeyManager::register_shortcut(const QKeySequence& keys, const QString& action) {
    bindings_.append({keys, action});
}

bool HotkeyManager::handle_key(QKeyEvent* event) {
    QKeySequence pressed(event->key() | (event->modifiers() & Qt::ShiftModifier ? Qt::ShiftModifier : 0)
                         | (event->modifiers() & Qt::ControlModifier ? Qt::ControlModifier : 0)
                         | (event->modifiers() & Qt::AltModifier ? Qt::AltModifier : 0));

    for (const auto& b : bindings_) {
        if (b.keys == pressed) {
            if (b.action == "fullscreen") emit fullscreen_toggled();
            else if (b.action == "undo") emit undo();
            else if (b.action == "redo") emit redo();
            else if (b.action == "screenshot") emit screenshot();
            else if (b.action == "toggle_drawing") emit toggle_drawing();
            else if (b.action == "zoom_in") emit zoom_in();
            else if (b.action == "zoom_out") emit zoom_out();
            else if (b.action == "scroll_left") emit scroll_left();
            else if (b.action == "scroll_right") emit scroll_right();
            return true;
        }
    }
    return false;
}

} // namespace fincept::ui
