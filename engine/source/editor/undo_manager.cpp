#include "editor/undo_manager.h"
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

    void UndoStack::Pop() {
        if (!undoStack.empty()) {
            undoStack.pop();
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

    void UndoManager::Push(UndoStack::Command& cmd) {
        const ImGuiID windowID = GetCurrentWindowID();
        Push(windowID, cmd);
    }

    void UndoManager::Push(ImGuiID windowID, UndoStack::Command& cmd) {
        UndoStack& history = windowCommands[windowID];

        if (incompleteCommand != nullptr) {
            CYB_WARNING("Overwriting a previous incomplete action");
            ClearIncompleteCommand();
        }

        history.Push(cmd);

        if (!cmd->IsComplete()) {
            incompleteCommand = cmd;
            incompleteCommandWindowID = windowID;
        }
    }

    void UndoManager::CommitIncompleteCommand() {
        if (!incompleteCommand) {
            return;
        }

        assert(incompleteCommand == windowCommands[incompleteCommandWindowID].Top());
        incompleteCommand->Complete();
        incompleteCommand.reset();
        incompleteCommandWindowID = 0;
    }

    void UndoManager::ClearIncompleteCommand() {
        if (!incompleteCommand) {
            return;
        }

        UndoStack& history = windowCommands[incompleteCommandWindowID];
        history.Pop();
        incompleteCommand.reset();
        incompleteCommandWindowID = 0;
    }

    void UndoManager::ClearHistory() {
        ClearIncompleteCommand();
        windowCommands.clear();
    }

    bool UndoManager::CanUndo() const {
        const UndoStack& history = GetHistoryForActiveWindow();
        return history.CanUndo();
    }

    bool UndoManager::CanRedo() const {
        const UndoStack& history = GetHistoryForActiveWindow();
        return history.CanRedo();
    }

    void UndoManager::Undo() {
        UndoStack& history = GetHistoryForActiveWindow();
        history.Undo();
    }

    void UndoManager::Redo() {
        UndoStack& history = GetHistoryForActiveWindow();
        history.Redo();
    }

    ImGuiID UndoManager::GetCurrentWindowID() const {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window) {
            return 0;
        }

        // get the toplevel window
        while (window->ParentWindow != nullptr) {
            window = window->ParentWindow;
        }

        return window->ID;
    }

    UndoStack& UndoManager::GetHistoryForActiveWindow() {
        const ImGuiID windowID = GetCurrentWindowID();
        return windowCommands[windowID];
    }

    const UndoStack& UndoManager::GetHistoryForActiveWindow() const {
        return const_cast<UndoManager*>(this)->GetHistoryForActiveWindow();
    }
}