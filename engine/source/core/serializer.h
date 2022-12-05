#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;

namespace cyb::serializer
{
    enum { kArchiveVersion = 4 };
    enum { kLeastUupportedVersion = 3 };
    enum { kArchiveInitSize = 128 };

    class Archive
    {
    public:
        enum Access
        {
            kRead,
            kWrite
        };

        Archive();                              // Open empty archive for writing
        Archive(const Archive&) = default;
        Archive(Archive&&) = default;

        // Open existing archive for reading
        Archive(const std::string& filename);
        ~Archive() { Close(); }
        Archive& operator=(const Archive&) = default;
        Archive& operator=(Archive&&) = default;

        void CreateEmpty();
        void SetAccessModeAndResetPos(Access mode);
        void Close();

        bool IsOpen() const;
        uint64_t GetVersion() const { return m_version; }
        bool IsReadMode() const { return m_mode == Access::kRead; }
        bool SaveFile(const std::string filename);

        //=============================================================
        //  Write Operations
        //=============================================================

        Archive& operator<<(char data);
        Archive& operator<<(int8_t data);
        Archive& operator<<(uint8_t data);
        Archive& operator<<(int32_t data);
        Archive& operator<<(uint32_t data);
        Archive& operator<<(int64_t data);
        Archive& operator<<(uint64_t data);
        Archive& operator<<(float data);
        Archive& operator<<(const std::string& data);
        Archive& operator<<(const XMFLOAT3& data);
        Archive& operator<<(const XMFLOAT4& data);
        Archive& operator<<(const XMFLOAT4X4& data);

        template <typename T>
        Archive& operator<<(std::vector<T>& data)
        {
            UnsafeWrite<size_t>(data.size());
            for (const T& x : data)
            {
                *(this) << x;
            }
            return *this;
        }

        //=============================================================
        //  Read Operations
        //=============================================================

        Archive& operator>>(char& data);
        Archive& operator>>(int8_t& data);
        Archive& operator>>(uint8_t& data);
        Archive& operator>>(int32_t& data);
        Archive& operator>>(uint32_t& data);
        Archive& operator>>(int64_t& data);
        Archive& operator>>(uint64_t& data);
        Archive& operator>>(float& data);
        Archive& operator>>(std::string& data);
        Archive& operator>>(XMFLOAT3& data);
        Archive& operator>>(XMFLOAT4& data);
        Archive& operator>>(XMFLOAT4X4& data);

        template <typename T>
        Archive& operator>>(std::vector<T>& data)
        {
            size_t count;
            UnsafeRead(count);
            data.resize(count);
            for (size_t i = 0; i < count; ++i)
            {
                *(this) >> data[i];
            }
            return *this;
        }

        // UnsafeWrite is used internally for serialization of custom
        // data and should be used with coution, use operator << instead
        template <typename T>
        inline void UnsafeWrite(const T& data)
        {
            assert(!IsReadMode());
            assert(!m_data.empty());
            size_t size = sizeof(data);
            size_t right = m_pos + size;
            if (right > m_data.size())
            {
                m_data.resize(right * 2);
                m_dataPtr = m_data.data();
            }

            *(T*)(m_data.data() + m_pos) = data;
            m_pos = right;
        }

        // UnsafeRead is used internally for serialization of custom
        // data and should be used with coution, use operator >> instead
        template <typename T>
        inline void UnsafeRead(T& data)
        {
            assert(IsReadMode());
            assert(m_dataPtr != nullptr);
            data = *(const T*)(m_dataPtr + m_pos);
            m_pos += (size_t)(sizeof(data));
        }

     private:
        uint64_t m_version = 0;
        Access m_mode = Access::kWrite;
        size_t m_pos = 0;
        std::vector<uint8_t> m_data;
        const uint8_t* m_dataPtr = nullptr;
    };
}