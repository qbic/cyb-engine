#include "core/serializer.h"
#include "lz4/lz4hc.h"

namespace cyb
{
    Archive::Archive(const std::span<uint8_t> data)
    {
        if (!data.empty())
        {
            // read mode
            m_readData = data.data();
            m_readDataLength = data.size();
        }
        else
        {
            // write mode
            m_writeBuffer.resize(1024 * 8);
            m_writeData = m_writeBuffer.data();
        }
    }

    bool Archive::IsReading() const
    {
        return m_readData != nullptr;
    }
    
    bool Archive::IsWriting() const
    {
        return m_writeData != nullptr;
    }

    std::span<const uint8_t> Archive::GetReadData() const
    {
        return std::span<const uint8_t>{ m_readData, Size() };
    }

    std::span<const uint8_t> Archive::GetWriteData() const
    {
        return std::span{ m_writeData, Size() };
    }
    
    size_t Archive::Size() const
    {
        return IsReading() ? m_readDataLength : m_position;
    }

    size_t Archive::Read(void* data, size_t length) const
    {
        assert(IsReading());
        assert(m_readData != nullptr);

        // check for overflow
        if (m_position + length > m_readDataLength)
            length = m_readDataLength - m_position;

        memcpy(data, m_readData + m_position, length);
        m_position += length;
        return length;
    }

#define READ_CHECK(value, length) { size_t bytesRead = Read(&value, length); assert(bytesRead = length); }

    void Archive::Read(char& value) const
    {
        READ_CHECK(value, sizeof(value));
    }

    void Archive::Read(uint8_t& value) const
    {
        READ_CHECK(value, sizeof(value));
    }

    void Archive::Read(uint32_t& value) const
    {
        READ_CHECK(value, sizeof(value));
    }

    void Archive::Read(uint64_t& value) const
    {
        READ_CHECK(value, sizeof(value));
    }

    void Archive::Read(float& value) const
    {
        READ_CHECK(value, sizeof(value));
    }

    void Archive::Read(std::string& value) const
    {
        size_t length = 0;
        Read(length);
        value.resize(length);
        READ_CHECK(value[0], length);
    }

    void Archive::Write(const void* data, size_t length)
    {
        assert(IsWriting());
        assert(m_writeData != nullptr);

        // dynamically stretch write buffer to fit new writes
        while (m_position + length > m_writeBuffer.size())
        {
            m_writeBuffer.resize((m_position + length) * 2);
            m_writeData = m_writeBuffer.data();
        }

        memcpy(m_writeData + m_position, data, length);
        m_position += length;
    }

    void Archive::Write(char value)
    {
        Write(&value, sizeof(value));
    }

    void Archive::Write(uint8_t value)
    {
        Write(&value, sizeof(value));
    }

    void Archive::Write(uint32_t value)
    {
        Write(&value, sizeof(value));
    }

    void Archive::Write(uint64_t value)
    {
        Write(&value, sizeof(value));
    }

    void Archive::Write(float value)
    {
        Write(&value, sizeof(value));
    }

    void Archive::Write(const std::string& str)
    {
        Write(str.length());
        Write((void*)str.data(), str.length());
    }

    Serializer::Serializer(Archive&& ar, int32_t version) :
        m_archive(std::move(ar)),
        m_version(version)
    {
    }

    bool Serializer::IsReading() const
    {
        return !m_archive.IsWriting();
    }
    
    bool Serializer::IsWriting() const
    {
        return m_archive.IsWriting();
    }
    
    uint32_t Serializer::GetVersion() const
    {
        return m_version;
    }

    std::span<const uint8_t> Serializer::GetArchiveData() const
    {
        return IsReading() ? m_archive.GetReadData() : m_archive.GetWriteData();
    }

    size_t Serializer::GetArchiveSize() const
    {
        return m_archive.Size();
    }

#define SERIALIZE_VALUE(value) { IsWriting() ? m_archive.Write(value) : m_archive.Read(value); }

    void Serializer::Serialize(char& value)
    {
        SERIALIZE_VALUE(value);
    }

    void Serializer::Serialize(uint8_t& value)
    {
        SERIALIZE_VALUE(value);
    }

    void Serializer::Serialize(uint32_t& value)
    {
        SERIALIZE_VALUE(value);
    }

    void Serializer::Serialize(uint64_t& value)
    {
        SERIALIZE_VALUE(value);
    }

    void Serializer::Serialize(float& value)
    {
        SERIALIZE_VALUE(value);
    }

    void Serializer::Serialize(std::string& value)
    {
        SERIALIZE_VALUE(value);
    }

    void Serializer::Serialize(XMFLOAT3& value)
    {
        if (IsWriting())
        {
            m_archive.Write(&value, sizeof(XMFLOAT3));
        }
        else
        {
            size_t bytesRead = m_archive.Read(&value, sizeof(XMFLOAT3));
            assert(bytesRead == sizeof(XMFLOAT3));
        }
    }

    void Serializer::Serialize(XMFLOAT4& value)
    {
        if (IsWriting())
        {
            m_archive.Write(&value, sizeof(XMFLOAT4));
        }
        else
        {
            size_t bytesRead = m_archive.Read(&value, sizeof(XMFLOAT4));
            assert(bytesRead == sizeof(XMFLOAT4));
        }
    }

    void Serializer::Serialize(XMFLOAT4X4& value)
    {
        if (IsWriting())
        {
            m_archive.Write(&value, sizeof(XMFLOAT4X4));
        }
        else
        {
            size_t bytesRead = m_archive.Read(&value, sizeof(XMFLOAT4X4));
            assert(bytesRead == sizeof(XMFLOAT4X4));
        }
    }

    void Compress(std::span<const uint8_t> source, std::vector<uint8_t>& dest)
    {
        constexpr int compressionLevel = 9; // 0..12
        dest.resize(LZ4_compressBound(source.size()));
        int res = LZ4_compress_HC(
            (const char*)source.data(),
            (char*)dest.data(),
            source.size(),
            dest.capacity(),
            compressionLevel
        );

        if (res <= 0)
        {
            dest.clear();
            return;
        }

        dest.resize(res);
    }

    void Decompress(std::span<const uint8_t> source, std::vector<uint8_t>& dest, uint64_t decompressedSize)
    {
        dest.resize(decompressedSize);
        int res = LZ4_decompress_safe(
            (const char*)source.data(),
            (char*)dest.data(),
            source.size(),
            decompressedSize
        );

        assert(res == decompressedSize);
        if (res <= 0)
            dest.clear();
    }
}