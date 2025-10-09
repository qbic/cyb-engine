#pragma once
#include <cassert>
#include <vector>
#include "core/non_copyable.h"

namespace cyb
{
    template <typename T>
    [[nodiscard]] T AlignPow2(const T value, const T align) noexcept
    {
        T result = (value + (align - 1)) & (~(align - 1));
        return result;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsPow2(T value) noexcept
    {
        const bool result = ((value & ~(value - 1)) == value);
        return result;
    }

    class ArenaAllocator : private NonCopyable
    {
    private:
        static constexpr size_t DEFAULT_BLOCK_SIZE{ 1024 * 1024 };

        struct Block
        {
            uint8_t* base;
            size_t capacity;
            size_t size;
        };

    public:
        ArenaAllocator() = default;
        
        ArenaAllocator(size_t minimumBlockSize, size_t alignment) :
            m_minimumBlockSize(minimumBlockSize),
            m_alignment(alignment)
        {
        }

        ~ArenaAllocator()
        {
            Clear();
        }

        // this must _NOT_ be called after any allocations to the arena
        void SetBlockSizeAndAlignment(size_t minimumBlockSize, size_t alignment)
        {
            assert(m_blocks.empty());
            m_minimumBlockSize = minimumBlockSize;
            m_alignment = alignment;
        }

        [[nodiscard]] uint8_t* Allocate(size_t size)
        {
            assert(m_alignment <= 128);
            assert(IsPow2(m_alignment));

            const size_t alignedSize = AlignPow2(size, m_alignment);
            Block& block = GetBlockForAllocation(alignedSize);
            uint8_t* result = block.base + block.size;
            block.size += alignedSize;

            // Update the LRU cache
            m_lastUsedBlock = &block;

            return result;
        }

        // Reset all blocks in the arena, but does not free any memory
        // Any memory referenced before Reset() is target for corruption!
        void Reset()
        {
            for (auto& block : m_blocks)
                block.size = 0;

            m_lastUsedBlock = nullptr;
        }

        // free all memory and clear all blocks
        void Clear()
        {
            for (auto& block : m_blocks)
                _aligned_free(block.base);

            m_blocks.clear();
        }

    private:
        [[nodiscard]] Block& GetBlockForAllocation(size_t allocationSize)
        {
            Block* block = FindBlockForAllocation(allocationSize);
            if (block != nullptr)
                return *block;
            return PushNewBlock(allocationSize);
        }

        [[nodiscard]] bool BlockCanAllocate(const Block& block, size_t allocationSize) const
        {
            return (block.size + allocationSize) < block.capacity;
        }

        [[nodiscard]] Block* FindBlockForAllocation(size_t allocationSize)
        {
            // Check LRU cache first.
            if (m_lastUsedBlock != nullptr && BlockCanAllocate(*m_lastUsedBlock, allocationSize))
                return m_lastUsedBlock;

            // Fallback to linear search.
            for (auto& block : m_blocks)
            {
                if (BlockCanAllocate(block, allocationSize))
                    return &block;
            }

            return nullptr;
        }

        Block& PushNewBlock(size_t blockSize)
        {
            if (!m_minimumBlockSize)
                m_minimumBlockSize = DEFAULT_BLOCK_SIZE;

            Block& block = m_blocks.emplace_back();
            block.capacity = std::max(blockSize, m_minimumBlockSize);
            block.size = 0;
            block.base = (uint8_t*)_aligned_malloc(block.capacity, m_alignment);

            return m_blocks.back();
        }

    private:
        std::vector<Block> m_blocks;
        Block* m_lastUsedBlock{ nullptr };      // Block LRU cache
        size_t m_alignment{ 1 };
        size_t m_minimumBlockSize{ 0 };         // if set to 0, DEFAULT_BLOCK_SIZE will be used
    };

    template <typename T>
    class ArenaStlProxy
    {
    public:
        using value_type = T;

        ArenaStlProxy() noexcept = default;
        ArenaStlProxy(ArenaAllocator* arena) noexcept : m_arena(arena) {}

        template <class U>
        ArenaStlProxy(const ArenaStlProxy<U>& other) noexcept :
            m_arena(other.m_arena)
        {
        }

        T* allocate(std::size_t n)
        {
            if (!m_arena)
                throw std::bad_alloc();
            
            T* ptr = reinterpret_cast<T*>(m_arena->Allocate(n * sizeof(T)));
            return ptr;
        }

        void deallocate(T* p, std::size_t) noexcept
        {
            // arena allocator never deletes memory
        }

        template <class U>
        bool operator==(const ArenaStlProxy<U>& b) const noexcept
        {
            return m_arena == b.m_arena;
        }

        template <class U>
        bool operator!=(const ArenaStlProxy<U>& b) const noexcept
        {
            return m_arena != b.m_arena;
        }

        ArenaAllocator* m_arena = nullptr;
    };
}