#include "editor/undo-manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cyb::ui
{
    void ActionHistory::PushAction(const std::shared_ptr<EditorAction>& action)
    {
        m_redoStack.clear();
        m_history.push_back(action);

        if (!action->IsComplete())
            m_incompleteAction = action;
    }

    void ActionHistory::CommitIncompleteAction()
    {
        m_incompleteAction->Complete(true);
        m_incompleteAction.reset();
    }

    void ActionHistory::Undo()
    {
        if (m_history.empty())
            return;

        std::shared_ptr<EditorAction> action = m_history.back();
        m_history.pop_back();
        action->Undo();
        m_redoStack.push_back(std::move(action));
    }

    void ActionHistory::Redo()
    {
        if (m_redoStack.empty())
            return;

        std::shared_ptr<EditorAction> action = m_redoStack.back();
        m_redoStack.pop_back();
        action->Redo();
        m_history.push_back(std::move(action));
    }

    void UndoManager::PushAction(const std::shared_ptr<EditorAction>& action)
    {
        ActionHistory& history = GetHistoryForActiveWindow();
        history.PushAction(action);
    }

    void UndoManager::CommitIncompleteAction()
    {
        ActionHistory& history = GetHistoryForActiveWindow();
        history.CommitIncompleteAction();
    }

    void UndoManager::Undo()
    {
        ActionHistory& history = GetHistoryForActiveWindow();
        history.Undo();
    }

    void UndoManager::Redo()
    {
        ActionHistory& history = GetHistoryForActiveWindow();
        history.Redo();
    }

    ActionHistory& UndoManager::GetHistoryForActiveWindow()
    {
        const ImGuiContext* g = ImGui::GetCurrentContext();
        ImGuiWindow* window = g->CurrentWindow;
        if (!window)
            return m_windowActions[0];

        // Get to the toplevel window
        while (window->ParentWindow != nullptr)
            window = window->ParentWindow;

        return m_windowActions[window->ID];
    }
}