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
        WindowActionHistory& history = windowCommands[windowID];

        if (history.incompleteCommand != nullptr) {
            CYB_WARNING("Overwriting a previous incomplete action");
            history.commands.Pop();
            history.incompleteCommand.reset();
        }

        history.commands.Push(cmd);

        if (!cmd->IsComplete()) {
            history.incompleteCommand = cmd;
        }
    }

    void UndoManager::CommitIncompleteCommand() {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        if (!history.incompleteCommand) {
            return;
        }

        assert(history.incompleteCommand == history.commands.Top());
        history.incompleteCommand->Complete();
        history.incompleteCommand.reset();
    }

    void UndoManager::ClearHistory() {
        WindowActionHistory& windowHistory = GetHistoryForActiveWindow();
        windowHistory.commands.Clear();
        windowHistory.incompleteCommand.reset();
    }

    bool UndoManager::CanUndo() const {
        const WindowActionHistory& history = GetHistoryForActiveWindow();
        return history.commands.CanUndo();
    }

    bool UndoManager::CanRedo() const {
        const WindowActionHistory& history = GetHistoryForActiveWindow();
        return history.commands.CanRedo();
    }

    void UndoManager::Undo() {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        history.commands.Undo();
    }

    void UndoManager::Redo() {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        history.commands.Redo();
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

    UndoManager::WindowActionHistory& UndoManager::GetHistoryForActiveWindow() {
        const ImGuiID windowID = GetCurrentWindowID();
        return windowCommands[windowID];
    }

    const UndoManager::WindowActionHistory& UndoManager::GetHistoryForActiveWindow() const {
        return const_cast<UndoManager*>(this)->GetHistoryForActiveWindow();
    }
}