#pragma once
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include "imgui/imgui.h"
#include "core/serializer.h"
#include "systems/scene.h"

namespace cyb::ui
{
    class EditorAction
    {
    public:
        virtual void Undo() const = 0;
        virtual void Redo() const = 0;

        virtual bool IsComplete() const { return true; }
        virtual void Complete(bool isComplete) {}
    };

    template <typename T, int N = 1>
    class EditorAction_ModifyValue : public EditorAction
    {
    public:
        using value_type = T;

        EditorAction_ModifyValue(T* dataPtr) :
            m_newValue()
        {
            assert(dataPtr);
            m_dataPtr = dataPtr;
            for (int i = 0; i < N; ++i)
                m_oldValue[i] = m_dataPtr[i];
            m_isComplete = false;
        }

        EditorAction_ModifyValue(T* dataPtr, const T newValue[N])
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

        virtual void Undo() const override
        {
            for (int i = 0; i < N; ++i)
                m_dataPtr[i] = m_oldValue[i];
        }

        virtual void Redo() const override
        {
            for (int i = 0; i < N; ++i)
                m_dataPtr[i] = m_newValue[i];
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
            EditorAction_ModifyValue(dataPtr)
        {
            m_transform = dataPtr;
        }

        void Undo() const override
        {
            EditorAction_ModifyValue::Undo();
            m_transform->SetDirty();
        }

        void Redo() const override
        {
            EditorAction_ModifyValue::Redo();
            m_transform->SetDirty();
        }

    private:
        scene::TransformComponent* m_transform;
    };

    class ActionHistory
    {
    public:
        void PushAction(const std::shared_ptr<EditorAction>& action);
        void CommitIncompleteAction();

        void Undo();
        void Redo();

    private:
        std::shared_ptr<EditorAction> m_incompleteAction;
        std::vector<std::shared_ptr<EditorAction>> m_history;
        std::vector<std::shared_ptr<EditorAction>> m_redoStack;
    };

    class UndoManager
    {
    public:
        void PushAction(const std::shared_ptr<EditorAction>& action);
        void CommitIncompleteAction();

        void Undo();
        void Redo();

    private:
        ActionHistory& GetHistoryForActiveWindow();

        std::unordered_map<ImGuiID, ActionHistory> m_windowActions;
    };

    inline UndoManager* GetUndoManager()
    {
        static UndoManager undoManager;
        return &undoManager;
    }
}