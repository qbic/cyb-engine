#pragma once
#include <unordered_map>
#include <stack>
#include <memory>
#include <functional>
#include "imgui/imgui.h"
#include "systems/scene.h"

namespace cyb::ui {
    using UndoCallback = std::function<void()>;

    class UndoCommand {
    public:
        // implementation should be cycling value (Undo()->Redo()->Undo()...)
        virtual void Undo() = 0;

        [[nodiscard]] virtual bool IsComplete() const { return true; }
        virtual void Complete() {}
    };

    template <typename T, int N = 1>
    class ModifyValue : public UndoCommand {
        static_assert(N >= 1 && N <= 1024, "N must be within the range [1, 1024]");
    public:
        using value_type = T;

        // construct a non-complete command
        ModifyValue(T* valuePtr, const UndoCallback& onChange = nullptr) :
            value(valuePtr),
            onChange(onChange),
            isComplete(false) {
            assert(value);

            for (int i = 0; i < N; ++i) {
                previousValue[i] = value[i];
            }
        }

        // construct a complete command
        ModifyValue(T* valuePtr, const T newValue[N], const UndoCallback& onChange = nullptr) :
            value(valuePtr),
            onChange(onChange),
            isComplete(true) {
            assert(value);

            for (int i = 0; i < N; ++i) {
                previousValue[i] = value[i];
                value[i] = newValue[i];
            }
        }

        virtual void Undo() override {
            for (int i = 0; i < N; ++i) {
                const T temp = value[i];
                value[i] = previousValue[i];
                previousValue[i] = temp;
            }

            if (onChange != nullptr) {
                onChange();
            }
        }

        [[nodiscard]] bool IsComplete() const override { return isComplete; }

        // calls onChange() if modify was not previously complete
        void Complete() override {
            if (isComplete) {
                return;
            }

            isComplete = true;
            if (onChange != nullptr) {
                onChange();
            }
        }

    private:
        UndoCallback onChange;
        T* value;
        T previousValue[N];
        bool isComplete;
    };

    class ModifyTransform : public ModifyValue<scene::TransformComponent> {
    public:
        ModifyTransform(scene::TransformComponent* valuePtr) :
            ModifyValue(valuePtr),
            m_transform(valuePtr) {
        }

        void Undo() override {
            ModifyValue::Undo();
            m_transform->SetDirty();
        }

    private:
        scene::TransformComponent* m_transform;
    };

    class UndoStack final {
    public:
        using Command = std::shared_ptr<UndoCommand>;

        void Push(Command& cmd);
        void Pop();
        [[nodiscard]] const Command& Top() const { return m_undoStack.top(); }
        void Clear();

        [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
        [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
        void Undo();
        void Redo();

    private:
        std::stack<Command> m_undoStack;
        std::stack<Command> m_redoStack;
    };

    // NOTE:
    // If emplacing or pushing a command that is not complete, it needs to be commited with
    // CommitIncompleteCommand() when complete or it will be popped from the undo stack.
    class UndoManager final {
    public:
        // emplace command to the current active window
        template <typename T, typename... Args>
        void Emplace(typename T::value_type* value, [[maybe_unused]] Args&&... args) {
            UndoStack::Command cmd = std::make_shared<T>(value, std::forward<Args>(args)...);
            Push(cmd);
        }

        template <typename T, typename... Args>
        void Emplace(ImGuiID windowID, typename T::value_type* value, [[maybe_unused]] Args&&... args) {
            UndoStack::Command cmd = std::make_shared<T>(value, std::forward<Args>(args)...);
            Push(windowID, cmd);
        }

        // push command to the current active window
        void Push(UndoStack::Command& cmd);

        void Push(ImGuiID windowID, UndoStack::Command& cmd);
        void CommitIncompleteCommand();
        void ClearIncompleteCommand();
        void ClearHistory();

        [[nodiscard]] bool CanUndo() const;
        [[nodiscard]] bool CanRedo() const;
        void Undo();
        void Redo();

    private:
        [[nodiscard]] ImGuiID GetCurrentWindowID() const;
        [[nodiscard]] UndoStack& GetHistoryForActiveWindow();
        [[nodiscard]] const UndoStack& GetHistoryForActiveWindow() const;

        UndoStack::Command incompleteCommand;
        ImGuiID incompleteCommandWindowID = 0;
        std::unordered_map<ImGuiID, UndoStack> windowCommands;
    };

    inline UndoManager& GetUndoManager() {
        static UndoManager undoManager;
        return undoManager;
    }
}