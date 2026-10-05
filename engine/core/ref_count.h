/*
 * @mainpage ref_count
 *
 * Intrusive reference counting for engine objects. The count lives inside the object,
 * so ownership can be shared and passed through raw-pointer APIs.
 *
 * Usage:
 *   class Texture : public RefCounter<ITexture> { ... };
 *
 *   auto tex = RefCountPtr<ITexture>::Create(new Texture());  // adopts initial ref
 *   RefCountPtr<ITexture> copy = tex;                         // AddRef
 *   RefCountPtr<ITexture> moved = std::move(tex);             // no count change
 *   ITexture* raw = moved.Get();                              // non-owning
 *   CreateTexture(&moved);                                    // out-param (T**)
 *
 * Notes:
 *   - RefCounter starts at 1, so wrap 'new' with Create()/Attach(), never AddRef.
 *   - operator& releases the current object; use GetAddressOf() to avoid that.
 *   - The interface must have a virtual destructor (RefCounter calls 'delete this').
 *   - The refcount is atomic, but a single RefCountPtr instance is not thread-safe.
 */
#pragma once
#include "core/non_copyable.h"
#include <cstdint>
#include <atomic>

namespace cyb
{
    class IResource : protected NonCopyableNonMovable
    {
    protected:
        IResource() = default;
        virtual ~IResource() = default;

    public:
        virtual uint32_t AddRef() = 0;
        virtual uint32_t Release() = 0;
        virtual uint32_t GetRefCount() = 0;
    };

    // RefCountPtr
    // Mostly a copy of Microsoft::WRL::ComPtr<T>
	template <typename T>
	class RefCountPtr
	{
	public:
        RefCountPtr() noexcept : m_ptr(nullptr)
        {
        }

        RefCountPtr(std::nullptr_t) noexcept
            : m_ptr(nullptr)
        {
        }

        RefCountPtr(const RefCountPtr& other) noexcept
            : m_ptr(other.m_ptr)
        {
            InternalAddRef();
        }

        // Copy ctor that allows to instanatiate class when U* is convertible to T*
        template<class U> 
            requires std::is_convertible_v<U*, T*>
        RefCountPtr(const RefCountPtr<U>& other) noexcept
            : m_ptr(other.m_ptr)
        {
            InternalAddRef();
        }

        RefCountPtr(RefCountPtr&& other) noexcept
            : m_ptr(nullptr)
        {
            Swap(other);
        }

        // Move ctor that allows instantiation of a class when U* is convertible to T*
        template<class U>
            requires std::is_convertible_v<U*, T*>
        RefCountPtr(RefCountPtr<U>&& other) noexcept
            : m_ptr(other.m_ptr)
        {
            other.m_ptr = nullptr;
        }

        ~RefCountPtr() noexcept
        {
            InternalRelease();
        }

        RefCountPtr& operator=(std::nullptr_t) noexcept
        {
            InternalRelease();
            return *this;
        }

        RefCountPtr& operator=(T* other) noexcept
        {
            if (m_ptr != other)
                RefCountPtr(other).Swap(*this);
            return *this;
        }

        template <typename U>
        RefCountPtr& operator=(U* other) noexcept
        {
            RefCountPtr(other).Swap(*this);
            return *this;
        }

        RefCountPtr& operator=(const RefCountPtr& other) noexcept
        {
            if (m_ptr != other.m_ptr)
                RefCountPtr(other).Swap(*this);
            return *this;
        }

        template<class U>
        RefCountPtr& operator=(const RefCountPtr<U>& other) noexcept
        {
            RefCountPtr(other).Swap(*this);
            return *this;
        }

        RefCountPtr& operator=(RefCountPtr&& other) noexcept
        {
            RefCountPtr(static_cast<RefCountPtr&&>(other)).Swap(*this);
            return *this;
        }

        template<class U>
        RefCountPtr& operator=(RefCountPtr<U>&& other) noexcept
        {
            RefCountPtr(static_cast<RefCountPtr<U>&&>(other)).Swap(*this);
            return *this;
        }

        void Swap(RefCountPtr&& r) noexcept
        {
            T* tmp = m_ptr;
            m_ptr = r.m_ptr;
            r.m_ptr = tmp;
        }

        void Swap(RefCountPtr& r) noexcept
        {
            T* tmp = m_ptr;
            m_ptr = r.m_ptr;
            r.m_ptr = tmp;
        }

        [[nodiscard]] T* Get() const noexcept
        {
            return m_ptr;
        }

        operator T* () const
        {
            return m_ptr;
        }

        T* operator->() const noexcept
        {
            return m_ptr;
        }

        T** operator&()
        {
            return ReleaseAndGetAddressOf();
        }

        [[nodiscard]] T* const* GetAddressOf() const noexcept
        {
            return &m_ptr;
        }

        [[nodiscard]] T** GetAddressOf() noexcept
        {
            return &m_ptr;
        }

        [[nodiscard]] T** ReleaseAndGetAddressOf() noexcept
        {
            InternalRelease();
            return &m_ptr;
        }

        T* Detach() noexcept
        {
            T* ptr = m_ptr;
            m_ptr = nullptr;
            return ptr;
        }

        // Set the pointer while keeping the object's reference count unchanged
        void Attach(T* other)
        {
            if (m_ptr != nullptr)
            {
                auto ref = m_ptr->Release();
                (void)ref;

                // Attaching to the same object only works if duplicate references are being coalesced. Otherwise
                // re-attaching will cause the pointer to be released and may cause a crash on a subsequent dereference.
                assert(ref != 0 || m_ptr != other);
            }

            m_ptr = other;
        }

        // Create a wrapper around a raw object while keeping the object's reference count unchanged
        static RefCountPtr<T> Create(T* other)
        {
            RefCountPtr<T> ptr;
            ptr.Attach(other);
            return ptr;
        }

        uint32_t Reset()
        {
            return InternalRelease();
        }

	protected:
        void InternalAddRef() const noexcept
        {
            if (m_ptr != nullptr)
                m_ptr->AddRef();
        }

        uint32_t InternalRelease() noexcept
        {
            uint32_t ref = 0;
            T* temp = m_ptr;

            if (temp != nullptr)
            {
                m_ptr = nullptr;
                ref = temp->Release();
            }

            return ref;
        }

	protected:
		T* m_ptr;
	};

    /*
     * A class that implements reference counting in a way compatible with RefCountPtr.
     * Intended usage is to use it as a base class for interface implementations, like so:
     * class Texture : public RefCounter<ITexture> { ... }
     */
    template<class T>
    class RefCounter : public T
    {
    private:
        std::atomic<uint32_t> m_refCount = 1;
    public:
        virtual uint32_t AddRef() override
        {
            return ++m_refCount;
        }

        virtual uint32_t Release() override
        {
            uint32_t result = --m_refCount;
            if (result == 0) {
                delete this;
            }
            return result;
        }

        virtual uint32_t GetRefCount() override
        {
            return m_refCount.load();
        }
    };
}