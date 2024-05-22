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
            components.reserve(reservedCount);
            entities.reserve(reservedCount);
            lookup.reserve(reservedCount);
        }

        ComponentManager(const ComponentManager&) = delete;
        ComponentManager& operator=(const ComponentManager&) = delete;

        // clear the container of all components and entities
        inline void Clear()
        {
            components.clear();
            entities.clear();
            lookup.clear();
        }

        // merge in an other component manager of the same type to this. 
        // the other component manager MUST NOT contain any of the same entities!
        // the other component manager is not retained after this operation!
        void Merge(ComponentManager<T>& other)
        {
            components.reserve(Size() + other.Size());
            entities.reserve(Size() + other.Size());
            lookup.reserve(Size() + other.Size());

            for (size_t i = 0; i < other.Size(); ++i)
            {
                Entity entity = other.entities[i];
                assert(!Contains(entity));
                entities.push_back(entity);
                lookup[entity] = components.size();
                components.push_back(other.components[i]);
            }

            other.Clear();
        }

        void Serialize(Serializer& ser, SceneSerializeContext& entitySerializer)
        {
            size_t componentCount = components.size();
            ser.Serialize(componentCount);
            if (ser.IsReading())
            {
                components.resize(componentCount);
                entities.resize(componentCount);
            }

            for (size_t i = 0; i < componentCount; ++i)
            {
                SerializeComponent(components[i], ser, entitySerializer);
            }

            for (size_t i = 0; i < componentCount; ++i)
            {
                SerializeEntity(entities[i], ser, entitySerializer);

                if (ser.IsReading())
                {
                    lookup[entities[i]] = i;
                }
            }
        }

        // ecs::INVALID_ENTITY is not allowed!
        // only one of this component type per entity is allowed!
        template <typename... Args>
        T& Create(Entity entity, Args&&... args)
        {
            assert(entity != INVALID_ENTITY);
            assert(lookup.find(entity) == lookup.end());
            assert(entities.size() == components.size());
            assert(lookup.size() == components.size());

            lookup[entity] = components.size();
            components.emplace_back(std::forward<Args>(args)...);
            entities.push_back(entity);

            return components.back();
        }

        void Remove(Entity entity)
        {
            auto it = lookup.find(entity);
            if (it == lookup.end())
                return;

            const size_t index = it->second;

            if (index < components.size() - 1)
            {
                std::exchange(components[index], components.back());
                entities[index] = entities.back();
                lookup[entities[index]] = index;
            }

            // shrink the container
            components.pop_back();
            entities.pop_back();
            lookup.erase(entity);
        }

        // check if a component exists for a given entity or not
        [[nodiscard]] bool Contains(Entity entity) const
        {
            return lookup.find(entity) != lookup.end();
        }

        // retrieve a [read/write] component specified by an entity (if it exists, otherwise nullptr)
        [[nodiscard]] T* GetComponent(Entity entity)
        {
            auto it = lookup.find(entity);
            return it != lookup.end() ? &components[it->second] : nullptr;
        }

        // retrieve a [read only] component specified by an entity (if it exists, otherwise nullptr)
        [[nodiscard]] const T* GetComponent(Entity entity) const
        {
            const auto it = lookup.find(entity);
            return it != lookup.end() ? &components[it->second] : nullptr;
        }

        // retrieve component index by entity handle (if not found, returns std::numeric_limits<size_t>::max)
        [[nodiscard]] size_t GetIndex(Entity entity) const
        {
            const auto it = lookup.find(entity);
            return it != lookup.end() ? it->second : std::numeric_limits<size_t>::max();
        }

        [[nodiscard]] Entity GetEntity(size_t index) const
        {
            return entities[index];
        }

        [[nodiscard]] size_t Size() const { return components.size(); }
        [[nodiscard]] T& operator[](size_t index) { return components[index]; }
        [[nodiscard]] const T& operator[](size_t index) const { return components[index]; }

    private:
        std::vector<T> components;
        std::vector<Entity> entities;
        ska::flat_hash_map<Entity, size_t> lookup;
    };
}