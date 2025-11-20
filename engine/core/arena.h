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
        static constexpr size_t defaultPageSize{ 1024 * 1024 };

        struct Page
        {
            uint8_t* base;
            size_t capacity;
            size_t size;
        };

    public:
        ArenaAllocator() = default;

        ArenaAllocator(size_t minPageSize, size_t alignment) :
            m_minPageSize(minPageSize),
            m_alignment(alignment)
        {
        }

        ~ArenaAllocator()
        {
            Clear();
        }

        // this must _NOT_ be called after any allocations to the arena
        void SetPageSizeAndAlignment(size_t minPageSize, size_t alignment)
        {
            assert(m_pages.empty());
            m_minPageSize = minPageSize;
            m_alignment = alignment;
        }

        [[nodiscard]] uint8_t* Allocate(size_t size)
        {
            assert(m_alignment <= 128);
            assert(IsPow2(m_alignment));

            const size_t alignedSize = AlignPow2(size, m_alignment);
            Page& page = GetPageForAllocation(alignedSize);
            uint8_t* result = page.base + page.size;
            page.size += alignedSize;

            // Update the LRU cache
            m_lastUsedPage = &page;

            return result;
        }

        // Reset all blocks in the arena, but does not free any memory
        // Any memory referenced before Reset() is target for corruption!
        void Reset()
        {
            for (auto& page : m_pages)
                page.size = 0;

            m_lastUsedPage = nullptr;
        }

        // free all memory and clear all blocks
        void Clear()
        {
            for (auto& page : m_pages)
                _aligned_free(page.base);

            m_pages.clear();
        }

    private:
        [[nodiscard]] Page& GetPageForAllocation(size_t allocationSize)
        {
            Page* page = FindPageForAllocation(allocationSize);
            if (page != nullptr)
                return *page;
            return PushNewPage(allocationSize);
        }

        [[nodiscard]] bool PageHasRoomForAllocation(const Page& page, size_t allocationSize) const
        {
            return (page.size + allocationSize) < page.capacity;
        }

        [[nodiscard]] Page* FindPageForAllocation(size_t allocationSize)
        {
            // Check LRU cache first.
            if (m_lastUsedPage != nullptr && PageHasRoomForAllocation(*m_lastUsedPage, allocationSize))
                return m_lastUsedPage;

            // Fallback to linear search.
            for (auto& page : m_pages)
            {
                if (PageHasRoomForAllocation(page, allocationSize))
                    return &page;
            }

            return nullptr;
        }

        Page& PushNewPage(size_t blockSize)
        {
            if (!m_minPageSize)
                m_minPageSize = defaultPageSize;

            Page& page = m_pages.emplace_back();
            page.capacity = std::max(blockSize, m_minPageSize);
            page.size = 0;
            page.base = (uint8_t*)_aligned_malloc(page.capacity, m_alignment);

            return m_pages.back();
        }

    private:
        std::vector<Page> m_pages;
        Page* m_lastUsedPage{ nullptr };    // LRU cache
        size_t m_alignment{ 0 };
        size_t m_minPageSize{ 0 };          // 0 == defaultPageSize
    };
} // namespace cyb