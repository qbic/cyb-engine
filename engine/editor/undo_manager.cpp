#include "editor/undo_manager.h"
#include "core/logger.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace cyb::ui
{
    void UndoStack::Push(Action cmd)
    {
        m_undoStack.push(cmd);
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

        auto cmd = m_undoStack.top();
        cmd->Undo();
        m_undoStack.pop();
        m_redoStack.push(cmd);
    }

    void UndoStack::Redo()
    {
        if (m_redoStack.empty())
            return;

        auto cmd = m_redoStack.top();
        cmd->Undo();
        m_redoStack.pop();
        m_undoStack.push(cmd);
    }

    void UndoStack::Clear()
    {
        while (!m_undoStack.empty())
            m_undoStack.pop();

        while (!m_redoStack.empty())
            m_redoStack.pop();
    }

    void UndoManager::PushAction(UndoStack::Action cmd)
    {
        const ImGuiID windowID = GetCurrentWindowID();
        PushAction(windowID, cmd);
    }

    void UndoManager::PushAction(ImGuiID windowID, UndoStack::Action cmd)
    {
        UndoStack& history = m_windowActions[windowID];

        if (m_incompleteAction != nullptr)
        {
            CYB_WARNING("Overwriting a previous incomplete action");
            ClearIncompleteAction();
        }

        if (!cmd->IsComplete())
        {
            m_incompleteAction = cmd;
            m_incompleteActionWindowID = windowID;
        }
        else
        {
            history.Push(cmd);
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