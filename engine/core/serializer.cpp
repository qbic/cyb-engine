#include "core/serializer.h"

namespace cyb
{
    Archive::Archive(const uint8_t* data, size_t length)
    {
        if (data != nullptr)
        {
            // read mode
            m_readData = data;
            m_readDataLength = length;
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

    const uint8_t* Archive::GetWriteData() const
    {
        return m_writeData;
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

#define READ_TYPE_IMPL(type, length) {          \
    type value;                                 \
    size_t bytesRead = Read(&value, length);    \
    assert(bytesRead == length);                \
    return value;                               \
}

#define READ_CHECK(value, length) {                         \
    size_t bytesRead = Read(&value, length); \
    assert(bytesRead = length);              \
}

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
        for (;;)
        {
            if (m_position + length >= m_readDataLength)
            {
                assert(0);
                break;
            }
            if (m_readData[m_position + length] == 0)
                break;
            length++;
        }

        value = std::string((const char*)m_readData + m_position, length + 1);
        m_position += length + 1;
    }

    void Archive::Write(void* data, size_t length)
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
        Write(&value, 1);
    }

    void Archive::Write(uint8_t value)
    {
        Write(&value, 1);
    }

    void Archive::Write(uint32_t value)
    {
        Write(&value, 4);
    }

    void Archive::Write(uint64_t value)
    {
        Write(&value, 8);
    }

    void Archive::Write(float value)
    {
        Write(&value, 4);
    }

    void Archive::Write(const std::string& str)
    {
        Write((void*)str.data(), str.length());
    }

    Serializer::Serializer(Archive& ar) :
        m_archive(&ar)
    {
        if (IsWriting())
        {
            m_header.version = ARCHIVE_VERSION;
            m_header.info.bits.compressed = 0;
        }

        Serialize(m_header.version);
        Serialize(m_header.info.raw);
    }

    bool Serializer::IsReading() const
    {
        return !m_archive->IsWriting();
    }
    
    bool Serializer::IsWriting() const
    {
        return m_archive->IsWriting();
    }
    
    uint32_t Serializer::GetVersion() const
    {
        return m_header.version;
    }

    void Serializer::Serialize(char& value)
    {
        if (IsWriting())
            m_archive->Write(value);
        else
            m_archive->Read(value);
    }

    void Serializer::Serialize(uint8_t& value)
    {
        if (IsWriting())
            m_archive->Write(value);
        else
            m_archive->Read(value);
    }

    void Serializer::Serialize(uint32_t& value)
    {
        if (IsWriting())
            m_archive->Write(value);
        else
            m_archive->Read(value);
    }

    void Serializer::Serialize(uint64_t& value)
    {
        if (IsWriting())
            m_archive->Write(value);
        else
            m_archive->Read(value);
    }

    void Serializer::Serialize(float& value)
    {
        if (IsWriting())
            m_archive->Write(value);
        else
            m_archive->Read(value);
    }

    void Serializer::Serialize(std::string& value)
    {
        if (IsWriting())
            m_archive->Write(value);
        else
            m_archive->Read(value);
    }

    void Serializer::Serialize(XMFLOAT3& value)
    {
        if (IsWriting())
        {
            m_archive->Write(&value, sizeof(XMFLOAT3));
        }
        else
        {
            size_t bytesRead = m_archive->Read(&value, sizeof(XMFLOAT3));
            assert(bytesRead == sizeof(XMFLOAT3));
        }
    }

    void Serializer::Serialize(XMFLOAT4& value)
    {
        if (IsWriting())
        {
            m_archive->Write(&value, sizeof(XMFLOAT4));
        }
        else
        {
            size_t bytesRead = m_archive->Read(&value, sizeof(XMFLOAT4));
            assert(bytesRead == sizeof(XMFLOAT4));
        }
    }

    void Serializer::Serialize(XMFLOAT4X4& value)
    {
        if (IsWriting())
        {
            m_archive->Write(&value, sizeof(XMFLOAT4X4));
        }
        else
        {
            size_t bytesRead = m_archive->Read(&value, sizeof(XMFLOAT4X4));
            assert(bytesRead == sizeof(XMFLOAT4X4));
        }
    }
}