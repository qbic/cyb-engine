#include "editor/undo-manager.h"
#include "core/logger.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cyb::ui
{
    void UndoManager::PushAction(const std::shared_ptr<EditorAction>& action)
    {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        
        history.redoStack.clear();
        history.actionBuffer.push_back(action);

        if (!action->IsComplete())
        {
            CYB_CWARNING(history.incompleteAction != nullptr, "Overwriting non-null incomplete action");
            history.incompleteAction = action;
        }
    }

    void UndoManager::CommitIncompleteAction()
    {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        
        history.incompleteAction->Complete(true);
        history.incompleteAction.reset();
    }

    void UndoManager::Undo()
    {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        
        if (history.actionBuffer.empty())
            return;

        std::shared_ptr<EditorAction> action = history.actionBuffer.back();
        history.actionBuffer.pop_back();
        action->OnUndo();
        history.redoStack.push_back(std::move(action));
    }

    void UndoManager::Redo()
    {
        WindowActionHistory& history = GetHistoryForActiveWindow();
        
        if (history.redoStack.empty())
            return;

        std::shared_ptr<EditorAction> action = history.redoStack.back();
        history.redoStack.pop_back();
        action->OnRedo();
        history.actionBuffer.push_back(std::move(action));
    }

    UndoManager::WindowActionHistory& UndoManager::GetHistoryForActiveWindow()
    {
        const ImGuiContext* g = ImGui::GetCurrentContext();
        ImGuiWindow* window = g->CurrentWindow;
        if (!window)
            return m_windowActions[0];

        // Get the toplevel window
        while (window->ParentWindow != nullptr)
            window = window->ParentWindow;

        return m_windowActions[window->ID];
    }
}