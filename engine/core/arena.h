#pragma once
#include <cassert>
#include <vector>
#include "core/non_copyable.h"

namespace cyb {

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
        minPageSize_(minPageSize),
        alignment_(alignment)
    {
    }

    ~ArenaAllocator()
    {
        Clear();
    }

    // this must _NOT_ be called after any allocations to the arena
    void SetPageSizeAndAlignment(size_t minPageSize, size_t alignment)
    {
        assert(pages_.empty());
        minPageSize_ = minPageSize;
        alignment_ = alignment;
    }

    [[nodiscard]] uint8_t* Allocate(size_t size)
    {
        assert(alignment_ <= 128);
        assert(IsPow2(alignment_));

        const size_t alignedSize = AlignPow2(size, alignment_);
        Page& page = GetPageForAllocation(alignedSize);
        uint8_t* result = page.base + page.size;
        page.size += alignedSize;

        // Update the LRU cache
        lastUsedPage_ = &page;

        return result;
    }

    // Reset all blocks in the arena, but does not free any memory
    // Any memory referenced before Reset() is target for corruption!
    void Reset()
    {
        for (auto& page : pages_)
            page.size = 0;

        lastUsedPage_ = nullptr;
    }

    // free all memory and clear all blocks
    void Clear()
    {
        for (auto& page : pages_)
            _aligned_free(page.base);

        pages_.clear();
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
        if (lastUsedPage_ != nullptr && PageHasRoomForAllocation(*lastUsedPage_, allocationSize))
            return lastUsedPage_;

        // Fallback to linear search.
        for (auto& page : pages_)
        {
            if (PageHasRoomForAllocation(page, allocationSize))
                return &page;
        }

        return nullptr;
    }

    Page& PushNewPage(size_t blockSize)
    {
        if (!minPageSize_)
            minPageSize_ = defaultPageSize;

        Page& page = pages_.emplace_back();
        page.capacity = std::max(blockSize, minPageSize_);
        page.size = 0;
        page.base = (uint8_t*)_aligned_malloc(page.capacity, alignment_);

        return pages_.back();
    }

private:
    std::vector<Page> pages_;
    Page* lastUsedPage_{ nullptr };     // LRU cache
    size_t alignment_{ 0 };
    size_t minPageSize_{ 0 };           // 0 == defaultPageSize
};

} // namespace cyb