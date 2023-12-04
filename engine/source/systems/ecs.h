#pragma once
#include <unordered_map>
#include <atomic>
#include "core/random.h"
#include "core/serializer.h"
#include "systems/job-system.h"

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
        uint64_t fileVersion = 5;
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
            {
                return;
            }

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
    void SerializeComponent(T& x, Archive& arhive, ecs::SceneSerializeContext& serialize)
    {
        assert(0 && "ecs::SerializeComponent must be implemented for all ecs components");
    }

    template <typename ComponentType>
    class ComponentManager
    {
    public:
        ComponentManager(size_t reservedCount = 0)
        {
            m_components.reserve(reservedCount);
            m_entities.reserve(reservedCount);
            m_lookup.reserve(reservedCount);
        }

        // Clear the container of all components and entities
        inline void Clear()
        {
            m_components.clear();
            m_entities.clear();
            m_lookup.clear();
        }

        // Merge in an other component manager of the same type to this. 
        // The other component manager MUST NOT contain any of the same entities!
        // The other component manager is not retained after this operation!
        inline void Merge(ComponentManager<ComponentType>& other)
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

        inline void Serialize(Serializer& ser, SceneSerializeContext& entitySerializer)
        {
            Archive& archive = ser.GetArchive();

            if (archive.IsReading())
            {
                // Clear component manager before deserializeing
                Clear();

                uint64_t count;
                archive >> count;

                m_components.resize(count);
                for (uint64_t i = 0; i < count; ++i)
                    SerializeComponent(m_components[i], ser, entitySerializer);

                m_entities.resize(count);
                for (uint64_t i = 0; i < count; ++i)
                {
                    Entity entity;

                    SerializeEntity(entity, ser, entitySerializer);
                    m_entities[i] = entity;
                    m_lookup[entity] = i;
                }
            }
            else
            {
                archive << (uint64_t)m_components.size();
                for (auto& component : m_components)
                    SerializeComponent(component, ser, entitySerializer);

                for (auto& entity : m_entities)
                    SerializeEntity(entity, ser, entitySerializer);
            }
        }

        inline ComponentType& Create(Entity entity)
        {
            // ecs::INVALID_ENTITY is not allowed!
            // Only one of this component type per entity is allowed!
            // Entity count must always be the same as the number of coponents!
            assert(entity != INVALID_ENTITY);
            assert(m_lookup.find(entity) == m_lookup.end());
            assert(m_entities.size() == m_components.size());
            assert(m_lookup.size() == m_components.size());

            m_lookup[entity] = m_components.size();
            m_components.push_back(ComponentType());
            m_entities.push_back(entity);

            return m_components.back();
        }

        inline void Remove(Entity entity)
        {
            auto it = m_lookup.find(entity);
            if (it != m_lookup.end()) 
            {
                // Directly index into components and entities array
                const size_t index = it->second;
                const Entity indexedEntity = m_entities[index];

                if (index < m_components.size() - 1) 
                {
                    // Swap out the dead element with the last one
                    m_components[index] = std::move(m_components.back());
                    m_entities[index] = m_entities.back();

                    // Update the lookup table
                    m_lookup[m_entities[index]] = index;
                }

                // Shrink the container
                m_components.pop_back();
                m_entities.pop_back();
                m_lookup.erase(indexedEntity);
            }
        }

        // Check if a component exists for a given entity or not
        inline bool Contains(Entity entity) const
        {
            return m_lookup.find(entity) != m_lookup.end();
        }

        // Retrieve a [read/write] component specified by an entity (if it exists, otherwise nullptr)
        inline ComponentType* GetComponent(Entity entity)
        {
            auto it = m_lookup.find(entity);
            if (it != m_lookup.end()) 
                return &m_components[it->second];

            return nullptr;
        }

        // Retrieve a [read only] component specified by an entity (if it exists, otherwise nullptr)
        inline const ComponentType* GetComponent(Entity entity) const
        {
            const auto it = m_lookup.find(entity);
            if (it != m_lookup.end()) 
                return &m_components[it->second];

            return nullptr;
        }

        // Retrieve component index by entity handle (if not exists, returns SIZE_MAX value)
        inline size_t GetIndex(Entity entity) const
        {
            const auto it = m_lookup.find(entity);
            if (it != m_lookup.end()) 
                return it->second;

            return SIZE_MAX;
        }

        inline Entity GetEntity(size_t index) const 
        { 
            return m_entities[index]; 
        }

        inline size_t Size() const { return m_components.size(); }
        inline ComponentType& operator[](size_t index) { return m_components[index]; }
        inline const ComponentType& operator[](size_t index) const { return m_components[index]; }

    private:
        std::vector<ComponentType> m_components;
        std::vector<Entity> m_entities;
        std::unordered_map<Entity, size_t> m_lookup;

        // Disallow this to be copied by mistake
        ComponentManager(const ComponentManager&) = delete;
    };
}