#include "editor/undo-manager.h"
#include "core/logger.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cyb::ui {

    void UndoStack::Push(Command& cmd) {
        undoStack.push(cmd);
        while (!redoStack.empty()) {
            redoStack.pop();
        }
    }

    void UndoStack::Undo() {
        if (undoStack.empty()) {
            return;
        }

        auto cmd = undoStack.top();
        cmd->Undo();
        undoStack.pop();
        redoStack.push(cmd);
    }

    void UndoStack::Redo() {
        if (redoStack.empty()) {
            return;
        }

        auto cmd = redoStack.top();
        cmd->Undo();
        redoStack.pop();
        undoStack.push(cmd);
    }

    void UndoStack::Clear() {
        while (!undoStack.empty()) {
            undoStack.pop();
        }

        while (!redoStack.empty()) {
            redoStack.pop();
        }
    }

    void UndoManager::Record(UndoStack::Command& cmd) {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        
        history.commands.Push(cmd);

        if (!cmd->IsComplete()) {
            if (history.incompleteCommand != nullptr) {
                CYB_WARNING("Overwriting a previous incomplete action");
            }
            history.incompleteCommand = cmd;
        }
    }

    void UndoManager::CommitIncompleteCommand() {
        WindowActionHistory& windowHistory = GetHistoryForActiveWindow();
        if (!windowHistory.incompleteCommand) {
            return;
        }

        windowHistory.incompleteCommand->SetComplete(true);
        windowHistory.incompleteCommand.reset();
    }

    void UndoManager::ClearHistory() {
        WindowActionHistory& windowHistory = GetHistoryForActiveWindow();
        windowHistory.commands.Clear();
        windowHistory.incompleteCommand.reset();
    }

    void UndoManager::Undo() {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        history.commands.Undo();
    }

    void UndoManager::Redo() {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        history.commands.Redo();
    }

    UndoManager::WindowActionHistory& UndoManager::GetHistoryForActiveWindow() {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window) {
            return windowCommands[0];
        }

        // get the toplevel window
        while (window->ParentWindow != nullptr) {
            window = window->ParentWindow;
        }

        return windowCommands[window->ID];
    }
}