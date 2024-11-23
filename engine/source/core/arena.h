#pragma once
#include <cassert>
#include <vector>
#include "core/enum_flags.h"

namespace cyb {

    struct ArenaBlock {
        size_t size;
        uint8_t* base;
        size_t used;
    };

    struct ArenaStats {
        size_t numBlocks;
        size_t totalAllocatedMemory;
        size_t totalUsedMemory;
        size_t maxUsedMemory;
        size_t alignment;
        size_t minimumBlockSize;
    };

    template <typename T>
    [[nodiscard]] constexpr T AlignPow2(const T value, const T alignment) {
        T result = ((value + (alignment - 1)) & ~((value - value) + alignment - 1));
        return result;
    }

    template <typename T>
    [[nodiscard]] constexpr bool IsPow2(T value) {
        const bool result = ((value & ~(value - 1)) == value);
        return result;
    }

    class Arena {
    private:
        const size_t DEFAULT_BLOCK_SIZE = 1024 * 1024;

    public:
        Arena() = default;
        Arena(size_t minimumBlockSize, size_t alignment) :
            m_minimumBlockSize(minimumBlockSize),
            m_alignment(alignment) {
        }
        ~Arena() {
            Clear();
        }

        // this must _NOT_ be called after any allocations to the arena
        void SetBlockSizeAndAlignment(size_t minimumBlockSize, size_t alignment) {
            assert(m_blocks.empty());
            m_minimumBlockSize = minimumBlockSize;
            m_alignment = alignment;
        }

        [[nodiscard]] uint8_t* Allocate(size_t size) {
            assert(m_alignment <= 128);
            assert(IsPow2(m_alignment));

            size_t allocationSize = AlignPow2(size, m_alignment);

            // try to find a block with memory available for the allocation
            ArenaBlock* block = BlockForAllocation(allocationSize);
            if (!block) {
                // if no available block, create a new and try again
                AllocateNewBlock(size + m_alignment);
                return Allocate(size);
            }

            uint8_t* result = block->base + block->used;
            block->used += allocationSize;

            m_totalUsedMemory += allocationSize;
            m_maxUsedMemory = std::max(m_totalUsedMemory, m_maxUsedMemory);

            return result;
        }

        // reset all blocks in the arena, but does not free any memory
        void Reset() {
            for (auto& block : m_blocks) {
                block.used = 0;
            }

            m_lastUsedBlock = nullptr;
            m_totalUsedMemory = 0;
        }

        // free all memory and clear all blocks
        void Clear() {
            for (auto& block : m_blocks) {
                _aligned_free(block.base);
            }

            m_blocks.clear();
        }

        [[nodiscard]] ArenaStats GetStats() const {
            ArenaStats stats = {};
            stats.numBlocks = m_blocks.size();
            stats.totalAllocatedMemory = m_totalAllocatedMemory;
            stats.totalUsedMemory = m_totalUsedMemory;
            stats.maxUsedMemory = m_maxUsedMemory;
            stats.alignment = m_alignment;
            stats.minimumBlockSize = m_minimumBlockSize;
            return stats;
        }

    private:
        [[nodiscard]] ArenaBlock* BlockForAllocation(size_t allocationSize) {
            const auto hasFreespace = [](const ArenaBlock* block, size_t size) -> bool {
                return (block->used + size) <= block->size;
            };

            // check last used block first
            if (m_lastUsedBlock != nullptr && hasFreespace(m_lastUsedBlock, allocationSize)) {
                return m_lastUsedBlock;
            }

            // fallback to linear search
            for (auto& block : m_blocks) {
                if (hasFreespace(&block, allocationSize)) {
                    m_lastUsedBlock = &block;
                    return &block;
                }
            }

            return nullptr;
        }
        
        void AllocateNewBlock(size_t blockSize) {
            if (!m_minimumBlockSize) {
                m_minimumBlockSize = DEFAULT_BLOCK_SIZE;
            }

            ArenaBlock& block = m_blocks.emplace_back();
            block.size = std::max(blockSize, m_minimumBlockSize);
            block.used = 0;
            block.base = (uint8_t*)_aligned_malloc(block.size, m_alignment);

            // we assume no previous block fit the allocation and
            // update the last used cache to the new block
            m_lastUsedBlock = &block;

            m_totalAllocatedMemory += AlignPow2(block.size, m_alignment);
        }

    private:
        std::vector<ArenaBlock> m_blocks;
        ArenaBlock* m_lastUsedBlock = nullptr;  // cache last used block
        size_t m_alignment = 1;
        size_t m_minimumBlockSize = 0;          // if set to 0, DEFAULT_BLOCK_SIZE will be used

        size_t m_totalAllocatedMemory = 0;
        size_t m_totalUsedMemory = 0;
        size_t m_maxUsedMemory = 0;             // ignoring resets
    };

    template <typename T>
    class ArenaStlProxy {
    public:
        using value_type = T;

        ArenaStlProxy() = default;
        explicit ArenaStlProxy(Arena* arena) noexcept : m_arena(arena) {}

        template <typename U>
        ArenaStlProxy(const ArenaStlProxy<U>& other) noexcept : m_arena(other.m_arena) {}

        T* allocate(std::size_t n) {
            if (!m_arena) {
                throw std::bad_alloc();
            }
           return reinterpret_cast<T*>(m_arena->Allocate(n * sizeof(T)));
        }

        void deallocate(T* p, std::size_t) noexcept {
            // arena allocator never deletes memory
        }

    private:
        Arena* m_arena = nullptr;
    };
}