#pragma once
#include <unordered_map>
#include <stack>
#include <memory>
#include <functional>
#include "imgui/imgui.h"
#include "systems/scene.h"

namespace cyb::ui
{
    using UndoCallback = std::function<void()>;

    class UndoAction
    {
    public:
        // implementation should be cycling value (Undo()->Redo()->Undo()...)
        virtual void Undo() = 0;

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
        ModifyValue(T* valuePtr, const UndoCallback& onChange = nullptr) :
            value(valuePtr),
            onChange(onChange),
            isComplete(false)
        {
            assert(value);

            for (int i = 0; i < N; ++i)
            {
                previousValue[i] = value[i];
            }
        }

        // construct a complete command
        ModifyValue(T* valuePtr, const T newValue[N], const UndoCallback& onChange = nullptr) :
            value(valuePtr),
            onChange(onChange),
            isComplete(true)
        {
            assert(value);

            for (int i = 0; i < N; ++i)
            {
                previousValue[i] = value[i];
                value[i] = newValue[i];
            }
        }

        virtual void Undo() override
        {
            for (int i = 0; i < N; ++i)
            {
                const T temp = value[i];
                value[i] = previousValue[i];
                previousValue[i] = temp;
            }

            if (onChange != nullptr)
            {
                onChange();
            }
        }

        [[nodiscard]] bool IsComplete() const override { return isComplete; }

        // calls onChange() if modify was not previously complete
        void MarkAsComplete() override
        {
            if (isComplete)
            {
                return;
            }

            isComplete = true;
            if (onChange != nullptr)
            {
                onChange();
            }
        }

    private:
        UndoCallback onChange;
        T* value;
        T previousValue[N];
        bool isComplete;
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

        void Push(Action cmd);
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
            UndoStack::Action cmd = std::make_shared<T>(value, std::forward<Args>(args)...);
            PushAction(cmd);
        }

        template <typename T, typename... Args>
        void EmplaceAction(ImGuiID windowID, typename T::value_type* value, [[maybe_unused]] Args&&... args)
        {
            UndoStack::Action cmd = std::make_shared<T>(value, std::forward<Args>(args)...);
            PushAction(windowID, cmd);
        }

        void PushAction(UndoStack::Action cmd);
        void PushAction(ImGuiID windowID, UndoStack::Action cmd);
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