#include "editor/undo_manager.h"
#include "core/logger.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace cyb::ui
{
    void UndoStack::Push(Action action)
    {
        m_undoStack.push(action);
        while (!m_redoStack.empty())
            m_redoStack.pop();
    }

    void UndoStack::Pop()
    {
        if (!m_undoStack.empty())
            m_undoStack.pop();
    }

    void UndoStack::Undo()
    {
        if (m_undoStack.empty())
            return;

        auto action = m_undoStack.top();
        action->Undo();
        m_undoStack.pop();
        m_redoStack.push(action);
    }

    void UndoStack::Redo()
    {
        if (m_redoStack.empty())
            return;

        auto action = m_redoStack.top();
        action->Undo();
        m_redoStack.pop();
        m_undoStack.push(action);
    }

    void UndoStack::Clear()
    {
        while (!m_undoStack.empty())
            m_undoStack.pop();

        while (!m_redoStack.empty())
            m_redoStack.pop();
    }

    void UndoManager::PushAction(UndoStack::Action action)
    {
        const ImGuiID windowID = GetCurrentWindowID();
        PushAction(windowID, action);
    }

    void UndoManager::PushAction(ImGuiID windowID, UndoStack::Action action)
    {
        UndoStack& history = m_windowActions[windowID];

        if (m_incompleteAction != nullptr)
        {
            CYB_WARNING("Overwriting a previous incomplete action");
            ClearIncompleteAction();
        }

        if (!action->IsComplete())
        {
            m_incompleteAction = action;
            m_incompleteActionWindowID = windowID;
        }
        else
        {
            history.Push(action);
        }
    }

    void UndoManager::CommitIncompleteAction()
    {
        if (!m_incompleteAction)
            return;

        m_incompleteAction->MarkAsComplete();
        PushAction(m_incompleteActionWindowID, m_incompleteAction);

        ClearIncompleteAction();
    }

    void UndoManager::ClearIncompleteAction()
    {
        if (!m_incompleteAction)
            return;

        m_incompleteAction.reset();
        m_incompleteActionWindowID = 0;
    }

    void UndoManager::ClearHistory()
    {
        ClearIncompleteAction();
        m_windowActions.clear();
    }

    bool UndoManager::CanUndo() const
    {
        const UndoStack& history = GetHistoryForActiveWindow();
        return history.CanUndo();
    }

    bool UndoManager::CanRedo() const
    {
        const UndoStack& history = GetHistoryForActiveWindow();
        return history.CanRedo();
    }

    void UndoManager::Undo()
    {
        UndoStack& history = GetHistoryForActiveWindow();
        history.Undo();
    }

    void UndoManager::Redo()
    {
        UndoStack& history = GetHistoryForActiveWindow();
        history.Redo();
    }

    ImGuiID UndoManager::GetCurrentWindowID() const
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return 0;

        // get the toplevel window
        while (window->ParentWindow != nullptr)
            window = window->ParentWindow;

        return window->ID;
    }

    UndoStack& UndoManager::GetHistoryForActiveWindow()
    {
        const ImGuiID windowID = GetCurrentWindowID();
        return m_windowActions[windowID];
    }

    const UndoStack& UndoManager::GetHistoryForActiveWindow() const
    {
        return const_cast<UndoManager*>(this)->GetHistoryForActiveWindow();
    }
}