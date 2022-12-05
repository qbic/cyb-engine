#pragma once
#include "core/random.h"
#include "core/serializer.h"
#include "systems/job-system.h"
#include <unordered_map>
#include <atomic>

namespace cyb::ecs 
{
    using Entity = uint32_t;
    static constexpr Entity InvalidEntity = 0;

    inline Entity CreateEntity() 
    {
        static std::atomic<Entity> next = InvalidEntity + 1;
        return next.fetch_add(1);
    }

    struct SerializeEntityContext
    {
        jobsystem::Context ctx;
        std::unordered_map<uint32_t, Entity> remap;

        ~SerializeEntityContext()
        {
            jobsystem::Wait(ctx);
        }
    };

    inline void SerializeEntity(serializer::Archive& ar, Entity& entity, SerializeEntityContext& serialize)
    {
        if (ar.IsReadMode())
        {
            uint32_t mem;
            ar >> mem;

            if (mem == InvalidEntity)
            {
                entity = (Entity)mem;
                return;
            }

            auto it = serialize.remap.find(mem);
            if (it == serialize.remap.end())
            {
                entity = CreateEntity();
                serialize.remap[mem] = entity;
            }
            else
            {
                entity = it->second;
            }
        }
        else 
        {
            ar << entity;
        }
    }

    template <typename ComponentType>
    class ComponentManager
    {
    public:
        ComponentManager(size_t reserved_count = 0)
        {
            components.reserve(reserved_count);
            entities.reserve(reserved_count);
            lookup.reserve(reserved_count);
        }

        // Clear the container of all components and entities
        inline void Clear()
        {
            components.clear();
            entities.clear();
            lookup.clear();
        }

        // Merge in an other component manager of the same type to this. 
        // The other component manager MUST NOT contain any of the same entities!
        // The other component manager is not retained after this operation!
        inline void Merge(ComponentManager<ComponentType>& other)
        {
            components.reserve(Size() + other.Size());
            entities.reserve(Size() + other.Size());
            lookup.reserve(Size() + other.Size());

            for (size_t i = 0; i < other.Size(); ++i) 
            {
                Entity entity = other.entities[i];
                //assert(!Contains(entity));
                entities.push_back(entity);
                lookup[entity] = components.size();
                components.push_back(other.components[i]);
            }

            other.Clear();
        }

        inline void Serialize(serializer::Archive& ar, SerializeEntityContext& serialize)
        {
            if (ar.IsReadMode()) 
            {
                // Clear component manager before deserializeing
                Clear();

                uint64_t count;
                ar >> count;

                components.resize(count);
                for (uint64_t i = 0; i < count; ++i) 
                {
                    scene::SerializeComponent(components[i], ar, serialize);
                }

                entities.resize(count);
                for (uint64_t i = 0; i < count; ++i) 
                {
                    Entity entity;
                    SerializeEntity(ar, entity, serialize);
                    entities[i] = entity;
                    lookup[entity] = i;
                }
            } else {
                ar << (uint64_t)components.size();
                for (auto& component : components) 
                {
                    scene::SerializeComponent(component, ar, serialize);
                }

                for (auto& entity : entities)
                {
                    SerializeEntity(ar, entity, serialize);
                }
            }
        }

        inline ComponentType& Create(Entity entity)
        {
            // ecs::INVALID_ENTITY is not allowed!
            // Only one of this component type per entity is allowed!
            // Entity count must always be the same as the number of coponents!
            assert(entity != InvalidEntity);
            assert(lookup.find(entity) == lookup.end());
            assert(entities.size() == components.size());
            assert(lookup.size() == components.size());

            lookup[entity] = components.size();
            components.push_back(ComponentType());
            entities.push_back(entity);

            return components.back();
        }

        inline void Remove(Entity entity)
        {
            auto it = lookup.find(entity);
            if (it != lookup.end()) 
            {
                // Directly index into components and entities array:
                const size_t index = it->second;
                assert(entity == entities[index]);
                //const Entity entity = entities[index];

                if (index < components.size() - 1) 
                {
                    // Swap out the dead element with the last one:
                    components[index] = std::move(components.back()); // try to use move instead of copy
                    entities[index] = entities.back();

                    // Update the lookup table:
                    lookup[entities[index]] = index;
                }

                // Shrink the container:
                components.pop_back();
                entities.pop_back();
                lookup.erase(entity);
            }
        }

        // Check if a component exists for a given entity or not
        inline bool Contains(Entity entity) const
        {
            return lookup.find(entity) != lookup.end();
        }

        // Retrieve a [read/write] component specified by an entity (if it exists, otherwise nullptr)
        inline ComponentType* GetComponent(Entity entity)
        {
            auto it = lookup.find(entity);
            if (it != lookup.end()) 
            {
                return &components[it->second];
            }

            return nullptr;
        }

        // Retrieve a [read only] component specified by an entity (if it exists, otherwise nullptr)
        inline const ComponentType* GetComponent(Entity entity) const
        {
            const auto it = lookup.find(entity);
            if (it != lookup.end()) 
            {
                return &components[it->second];
            }

            return nullptr;
        }

        // Retrieve component index by entity handle (if not exists, returns SIZE_MAX value)
        inline size_t GetIndex(Entity entity) const
        {
            const auto it = lookup.find(entity);
            if (it != lookup.end()) 
            {
                return it->second;
            }

            return SIZE_MAX;
        }

        inline Entity GetEntity(size_t index) const 
        { 
            return entities[index]; 
        }

        inline size_t Size() const { return components.size(); }
        inline ComponentType& operator[](size_t index) { return components[index]; }
        inline const ComponentType& operator[](size_t index) const { return components[index]; }

    private:
        std::vector<ComponentType> components;
        std::vector<Entity> entities;
        std::unordered_map<Entity, size_t> lookup;

        // Disallow this to be copied by mistake
        ComponentManager(const ComponentManager&) = delete;
    };
}