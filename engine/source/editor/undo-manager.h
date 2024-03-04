#pragma once
#include <unordered_map>
#include <stack>
#include <memory>
#include <functional>
#include "imgui/imgui.h"
#include "systems/scene.h"

namespace cyb::ui {

    using UndoCallback = std::function<void()>;

    // Implementation details for UndoCommand:
    //   * value_type must be defined
    //   * Undo() should be cycling value (Undo() -> Redo() -> Undo()...)
    class UndoCommand {
    public:
        virtual void Undo() = 0;

        [[nodiscard]] virtual bool IsComplete() const { return true; }
        virtual void SetComplete(bool complete) {}
    };

    template <typename T, int N = 1>
    class ModifyValue : public UndoCommand {
        static_assert(N >= 1 && N <= 1024, "N must be within the range [1, 1024]");
    public:
        using value_type = T;

        // construct a non-complete command
        ModifyValue(T* valuePtr, UndoCallback onChange = nullptr) :
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

        // also calls onChange() if madify is complete
        void SetComplete(bool complete) override {
            isComplete = complete;
            if (isComplete && onChange != nullptr) {
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
            transform(valuePtr) {
        }

        void Undo() override {
            ModifyValue::Undo();
            transform->SetDirty();
        }

    private:
        scene::TransformComponent* transform;
    };

    class UndoStack final {
    public:
        using Command = std::shared_ptr<UndoCommand>;

        [[nodiscard]] bool CanUndo() const { return !undoStack.empty(); }
        [[nodiscard]] bool CanRedo() const { return !redoStack.empty(); }
        void Push(Command& cmd);
        void Undo();
        void Redo();
        void Clear();

    private:
        std::stack<Command> undoStack;
        std::stack<Command> redoStack;
    };

    class UndoManager final {
    public:
        void Record(UndoStack::Command& cmd);
        void CommitIncompleteCommand();
        void ClearHistory();

        void Undo();
        void Redo();

    private:
        struct WindowActionHistory {
            UndoStack::Command incompleteCommand;
            UndoStack commands;
        };

        WindowActionHistory& GetHistoryForActiveWindow();

        std::unordered_map<ImGuiID, WindowActionHistory> windowCommands;
    };

    inline UndoManager& GetUndoManager() {
        static UndoManager undoManager;
        return undoManager;
    }
}