#pragma once
#include <atomic>
#include "core/serializer.h"
#include "systems/job_system.h"
#include "flat_hash_map.hpp"

namespace cyb::ecs
{
    using Entity = uint32_t;
    static constexpr Entity INVALID_ENTITY = 0;

    inline Entity CreateEntity()
    {
        static std::atomic<Entity> next = INVALID_ENTITY + 1;
        return next.fetch_add(1);
    }

    struct SceneSerializeContext
    {
        uint64_t archiveVersion = 0;
        jobsystem::Context ctx;
        std::unordered_map<uint32_t, Entity> remap;

        ~SceneSerializeContext()
        {
            jobsystem::Wait(ctx);
        }
    };

    inline void SerializeEntity(Entity& entity, Serializer& ser, SceneSerializeContext& serialize)
    {
        ser.Serialize(entity);

        if (ser.IsReading())
        {
            if (entity == INVALID_ENTITY)
                return;

            auto it = serialize.remap.find(entity);
            if (it == serialize.remap.end())
            {
                Entity newEntity = CreateEntity();
                serialize.remap[entity] = newEntity;
                entity = newEntity;
            }
            else
            {
                entity = it->second;
            }
        }
    }

    template <typename T>
    class ComponentManager
    {
    public:
        ComponentManager(size_t reservedCount = 0)
        {
            m_components.reserve(reservedCount);
            m_entities.reserve(reservedCount);
            m_lookup.reserve(reservedCount);
        }

        ComponentManager(const ComponentManager&) = delete;
        ComponentManager& operator=(const ComponentManager&) = delete;

        // clear the container of all components and entities
        inline void Clear()
        {
            m_components.clear();
            m_entities.clear();
            m_lookup.clear();
        }

        // merge in an other component manager of the same type to this. 
        // the other component manager MUST NOT contain any of the same entities!
        // the other component manager is not retained after this operation!
        void Merge(ComponentManager<T>& other)
        {
            m_components.reserve(Size() + other.Size());
            m_entities.reserve(Size() + other.Size());
            m_lookup.reserve(Size() + other.Size());

            for (size_t i = 0; i < other.Size(); ++i)
            {
                Entity entity = other.m_entities[i];
                assert(!Contains(entity));
                m_entities.push_back(entity);
                m_lookup[entity] = m_components.size();
                m_components.push_back(other.m_components[i]);
            }

            other.Clear();
        }

        void Serialize(Serializer& ser, SceneSerializeContext& entitySerializer)
        {
            size_t componentCount = m_components.size();
            ser.Serialize(componentCount);
            if (ser.IsReading())
            {
                m_components.resize(componentCount);
                m_entities.resize(componentCount);
            }

            for (size_t i = 0; i < componentCount; ++i)
            {
                SerializeComponent(m_components[i], ser, entitySerializer);
            }

            for (size_t i = 0; i < componentCount; ++i)
            {
                SerializeEntity(m_entities[i], ser, entitySerializer);

                if (ser.IsReading())
                    m_lookup[m_entities[i]] = i;
            }
        }

        // ecs::INVALID_ENTITY is not allowed!
        // only one of this component type per entity is allowed!
        template <typename... Args,
            typename std::enable_if < std::is_constructible<T, Args&&...>{}, bool > ::type = true >
        T& Create(Entity entity, Args&&... args)
        {
            assert(entity != INVALID_ENTITY);
            assert(m_lookup.find(entity) == m_lookup.end());
            assert(m_entities.size() == m_components.size());
            assert(m_lookup.size() == m_components.size());

            m_lookup[entity] = m_components.size();
            m_entities.push_back(entity);
            return m_components.emplace_back(std::forward<Args>(args)...);
        }

        void Remove(Entity entity)
        {
            auto it = m_lookup.find(entity);
            if (it == m_lookup.end())
                return;

            const size_t index = it->second;

            if (index < m_components.size() - 1)
            {
                std::swap(m_components[index], m_components.back());
                m_entities[index] = m_entities.back();
                m_lookup[m_entities[index]] = index;
            }

            // shrink the container
            m_components.pop_back();
            m_entities.pop_back();
            m_lookup.erase(entity);
        }

        // check if a component exists for a given entity or not
        [[nodiscard]] bool Contains(Entity entity) const
        {
            return m_lookup.find(entity) != m_lookup.end();
        }

        // retrieve a [read/write] component specified by an entity (if it exists, otherwise nullptr)
        [[nodiscard]] T* GetComponent(Entity entity)
        {
            auto it = m_lookup.find(entity);
            return it != m_lookup.end() ? &m_components[it->second] : nullptr;
        }

        // retrieve a [read only] component specified by an entity (if it exists, otherwise nullptr)
        [[nodiscard]] const T* GetComponent(Entity entity) const
        {
            const auto it = m_lookup.find(entity);
            return it != m_lookup.end() ? &m_components[it->second] : nullptr;
        }

        // retrieve component index by entity handle (if not found, returns std::numeric_limits<size_t>::max)
        [[nodiscard]] size_t GetIndex(Entity entity) const
        {
            const auto it = m_lookup.find(entity);
            return it != m_lookup.end() ? it->second : std::numeric_limits<size_t>::max();
        }

        [[nodiscard]] Entity GetEntity(size_t index) const
        {
            return m_entities[index];
        }

        [[nodiscard]] size_t Size() const { return m_components.size(); }
        [[nodiscard]] T& operator[](size_t index) { return m_components[index]; }
        [[nodiscard]] const T& operator[](size_t index) const { return m_components[index]; }
        [[nodiscard]] auto begin() noexcept { return m_components.begin(); }
        [[nodiscard]] auto end() noexcept { return m_components.end(); }
        [[nodiscard]] auto begin() const noexcept { return m_components.begin(); }
        [[nodiscard]] auto end() const noexcept { return m_components.end(); }

    private:
        std::vector<T> m_components;
        std::vector<Entity> m_entities;
        ska::flat_hash_map<Entity, size_t> m_lookup;
    };
}