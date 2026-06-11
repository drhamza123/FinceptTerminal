#include "ui/drawing/DrawingTypes.h"

namespace fincept::ui {

DrawingUndoCommand::DrawingUndoCommand(Action action, const DrawingObject& before,
                                        const DrawingObject& after, int point_index)
    : QUndoCommand(), action_(action), before_(before), after_(after), point_index_(point_index) {
    switch (action) {
    case Add: setText(QObject::tr("Add drawing")); break;
    case Remove: setText(QObject::tr("Remove drawing")); break;
    case Modify: setText(QObject::tr("Modify drawing")); break;
    case MovePoint: setText(QObject::tr("Move point")); break;
    }
}

void DrawingUndoCommand::undo() {
    switch (action_) {
    case Add: /* remove after_ */ break;
    case Remove: /* restore before_ */ break;
    case Modify: /* restore before_ */ break;
    case MovePoint: /* restore point */ break;
    }
}

void DrawingUndoCommand::redo() {
    switch (action_) {
    case Add: /* add after_ */ break;
    case Remove: /* remove before_ */ break;
    case Modify: /* apply after_ */ break;
    case MovePoint: /* move point */ break;
    }
}

} // namespace fincept::ui
