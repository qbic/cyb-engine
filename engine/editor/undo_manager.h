#pragma once
#include <unordered_map>
#include <stack>
#include <memory>
#include <functional>
#include "imgui.h"
#include "systems/scene.h"

namespace cyb::ui
{
    using UndoCallback = std::function<void()>;

    class UndoAction
    {
    public:
        // implementation should be cycling the value, eg:
        // std::swap(previousValue[i], value[i]);
        virtual void Undo() {}

        [[nodiscard]] virtual bool IsComplete() const { return true; }
        virtual void MarkAsComplete() {}
    };

    template <typename T, int N = 1>
    class ModifyValue : public UndoAction
    {
        static_assert(N >= 1 && N <= 1024, "N must be within the range [1, 1024]");
    public:
        using value_type = T;

        // construct a non-complete command
        ModifyValue(T* value, const UndoCallback& onChange = nullptr) :
            m_value(value),
            m_onChange(onChange),
            m_isComplete(false)
        {
            assert(m_value);

            for (int i = 0; i < N; ++i)
                m_previousValue[i] = m_value[i];
        }

        // construct a complete command
        ModifyValue(T* value, const T newValue[N], const UndoCallback& onChange = nullptr) :
            m_value(value),
            m_onChange(onChange),
            m_isComplete(true)
        {
            assert(m_value);

            for (int i = 0; i < N; ++i)
            {
                m_previousValue[i] = m_value[i];
                m_value[i] = newValue[i];
            }
        }

        virtual void Undo() override
        {
            for (int i = 0; i < N; ++i)
                std::swap(m_previousValue[i], m_value[i]);

            if (m_onChange != nullptr)
                m_onChange();
        }

        [[nodiscard]] bool IsComplete() const override { return m_isComplete; }

        // calls onChange() if modify was not previously complete
        void MarkAsComplete() override
        {
            if (m_isComplete)
                return;

            m_isComplete = true;
            if (m_onChange != nullptr)
                m_onChange();
        }

    private:
        UndoCallback m_onChange;
        T* m_value;
        T m_previousValue[N];
        bool m_isComplete;
    };

    class ModifyTransform : public ModifyValue<scene::TransformComponent>
    {
    public:
        ModifyTransform(scene::TransformComponent* valuePtr) :
            ModifyValue(valuePtr),
            m_transform(valuePtr)
        {
        }

        void Undo() override
        {
            ModifyValue::Undo();
            m_transform->SetDirty();
        }

    private:
        scene::TransformComponent* m_transform;
    };

    class UndoStack final
    {
    public:
        using Action = std::shared_ptr<UndoAction>;

        void Push(Action action);
        void Pop();
        [[nodiscard]] const Action Top() const { return m_undoStack.top(); }
        void Clear();

        [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
        [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
        void Undo();
        void Redo();

    private:
        std::stack<Action> m_undoStack;
        std::stack<Action> m_redoStack;
    };

    // * all actions will be added their own per-window stack unless explicitly told so
    // * if adding an incomplete action, it won't be pushed to the undo stack 
    //   untill calling CommitIncompleteCommand() 
    class UndoManager final
    {
    public:
        template <typename T, typename... Args>
        void EmplaceAction(typename T::value_type* value, [[maybe_unused]] Args&&... args)
        {
            UndoStack::Action action = std::make_shared<T>(value, std::forward<Args>(args)...);
            PushAction(action);
        }

        template <typename T, typename... Args>
        void EmplaceAction(ImGuiID windowID, typename T::value_type* value, [[maybe_unused]] Args&&... args)
        {
            UndoStack::Action action = std::make_shared<T>(value, std::forward<Args>(args)...);
            PushAction(windowID, action);
        }

        void PushAction(UndoStack::Action action);
        void PushAction(ImGuiID windowID, UndoStack::Action action);
        void CommitIncompleteAction();
        void ClearIncompleteAction();
        void ClearHistory();

        [[nodiscard]] bool CanUndo() const;
        [[nodiscard]] bool CanRedo() const;
        void Undo();
        void Redo();

    private:
        [[nodiscard]] ImGuiID GetCurrentWindowID() const;
        [[nodiscard]] UndoStack& GetHistoryForActiveWindow();
        [[nodiscard]] const UndoStack& GetHistoryForActiveWindow() const;

        UndoStack::Action m_incompleteAction;
        ImGuiID m_incompleteActionWindowID = 0;
        std::unordered_map<ImGuiID, UndoStack> m_windowActions;
    };

    inline UndoManager& GetUndoManager()
    {
        static UndoManager undoManager;
        return undoManager;
    }
}