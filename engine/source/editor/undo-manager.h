#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include "imgui/imgui.h"
#include "systems/scene.h"

namespace cyb::ui
{
    class EditorAction
    {
    public:
        virtual void OnUndo() const = 0;
        virtual void OnRedo() const = 0;

        virtual bool IsComplete() const { return true; }
        virtual void Complete(bool isComplete) {}
    };

    template <typename T, int N = 1>
    class EditorAction_ModifyValue : public EditorAction
    {
    public:
        using value_type = T;
        static constexpr int num = N;

        EditorAction_ModifyValue(T* dataPtr, std::function<void()> onChange = nullptr) :
            m_onChange(onChange),
            m_newValue()
        {
            assert(dataPtr);
            m_dataPtr = dataPtr;
            for (int i = 0; i < N; ++i)
                m_oldValue[i] = m_dataPtr[i];
            m_isComplete = false;
        }

        EditorAction_ModifyValue(T* dataPtr, const T newValue[N], const std::function<void()>& onChange = nullptr) :
            m_onChange(onChange)
        {
            assert(dataPtr != nullptr);
            m_dataPtr = dataPtr;
            for (int i = 0; i < N; ++i)
            {
                m_oldValue[i] = m_dataPtr[i];
                m_newValue[i] = newValue[i];
            }
            m_isComplete = true;
        }

        virtual void OnUndo() const override
        {
            for (int i = 0; i < N; ++i)
                m_dataPtr[i] = m_oldValue[i];

            if (m_onChange)
                m_onChange();
        }

        virtual void OnRedo() const override
        {
            for (int i = 0; i < N; ++i)
                m_dataPtr[i] = m_newValue[i];

            if (m_onChange)
                m_onChange();
        }

        bool IsComplete() const override { return m_isComplete; }
        void Complete(bool isComplete) override
        { 
            if (isComplete)
            {
                for (int i = 0; i < N; ++i) 
                    m_newValue[i] = m_dataPtr[i];
            }
            m_isComplete = isComplete;
        }

    private:
        std::function<void()> m_onChange;
        T* m_dataPtr;
        T m_oldValue[N];
        T m_newValue[N];
        bool m_isComplete;
    };

    class EditorAction_ModifyTransform : public EditorAction_ModifyValue<scene::TransformComponent, 1>
    {
    public:
        using value_type = scene::TransformComponent;

        EditorAction_ModifyTransform(scene::TransformComponent* dataPtr) :
            EditorAction_ModifyValue(dataPtr),
            m_transform(dataPtr)
        {
        }

        void OnUndo() const override
        {
            EditorAction_ModifyValue::OnUndo();
            m_transform->SetDirty();
        }

        void OnRedo() const override
        {
            EditorAction_ModifyValue::OnRedo();
            m_transform->SetDirty();
        }

    private:
        scene::TransformComponent* m_transform;
    };

    // NOTE: All top-level windows has their own action history
    class UndoManager
    {
    public:
        void Record(const std::shared_ptr<EditorAction>& action);
        void CommitIncompleteAction();
        void ClearActionHistory();

        void Undo();
        void Redo();

    private:
        struct WindowActionHistory
        {
            std::shared_ptr<EditorAction> incompleteAction;
            std::vector<std::shared_ptr<EditorAction>> actionBuffer;
            std::vector<std::shared_ptr<EditorAction>> redoStack;
        };

        WindowActionHistory& GetHistoryForActiveWindow();

        std::unordered_map<ImGuiID, WindowActionHistory> m_windowActions;
    };

    inline UndoManager& GetUndoManager()
    {
        static UndoManager undoManager;
        return undoManager;
    }
}