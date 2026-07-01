#pragma once
#include <cassert>
#include <vector>
#include "core/non_copyable.h"
#include "core/mathlib.h"

namespace cyb
{
    class ArenaAllocator : private NonCopyable
    {
    private:
        static constexpr size_t defaultPageSize{ 1024 * 1024 };

        struct Page
        {
            std::byte* base;
            size_t capacity;
            size_t size;
        };

    public:
        ArenaAllocator() = default;

        ArenaAllocator(size_t minPageSize, size_t alignment) :
            m_minPageSize(minPageSize),
            m_alignment(alignment)
        {
            m_minPageSize = m_minPageSize ? m_minPageSize : defaultPageSize;
        }

        ~ArenaAllocator()
        {
            Clear();
        }

        // this must _NOT_ be called after any allocations to the arena
        void SetPageSizeAndAlignment(size_t minPageSize, size_t alignment)
        {
            assert(m_pages.empty());
            m_minPageSize = m_minPageSize ? m_minPageSize : defaultPageSize;
            m_alignment = alignment;
        }

        [[nodiscard]] std::byte* Allocate(size_t size)
        {
            assert(m_alignment <= 128);

            const size_t alignedSize = AlignPow2(size, m_alignment);
            Page* page = FindPageForAllocation(alignedSize);
            if (!page)
            {
                // No page has enough room, so create a new one
                page = &m_pages.emplace_back();
                page->capacity = std::max(alignedSize, m_minPageSize);
                page->size = 0;

#if defined(_MSC_VER)
                page->base = static_cast<std::byte*>(_aligned_malloc(page->capacity, m_alignment));
#else
                page->base = static_cast<std::byte*>(std::aligned_alloc(m_alignment, page->capacity));
#endif
                if (!page->base)
                    throw std::bad_alloc{};
            }

            std::byte* result = page->base + page->size;
            page->size += alignedSize;

            // Update the LRU cache
            m_lastUsedPage = page;

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
            {
#if defined(_MSC_VER)
                _aligned_free(page.base);
#else
                std::free(page.base);
#endif
            }

            m_pages.clear();
            m_lastUsedPage = nullptr;
        }

    private:
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

    private:
        std::vector<Page> m_pages;
        Page* m_lastUsedPage{ nullptr };    // LRU cache
        size_t m_alignment{ 0 };
        size_t m_minPageSize{ 0 };          // 0 == defaultPageSize
    };
} // namespace cyb