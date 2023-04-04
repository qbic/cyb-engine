#include "core/logger.h"
#include "core/serializer.h"
#include "core/filesystem.h"

namespace cyb::serializer
{
    Archive::Archive()
    {
        CreateEmpty();
    }

    Archive::Archive(const std::string& filename) :
        m_mode(Access::Read)
    {
        if (filesystem::ReadFile(filename, m_data))
        {
            m_dataPtr = m_data.data();
            (*this) >> m_version;
            CYB_CERROR(m_version < LeastUupportedVersion, "Unsupported version file={} version={} LeastUupportedVersion={}", filename, m_version, LeastUupportedVersion);
        }
    }

    void Archive::CreateEmpty()
    {
        m_version = ArchiveVersion;
        m_data.resize(ArchiveInitSize);
        m_dataPtr = m_data.data();
        SetAccessModeAndResetPos(Write);
    }

    void Archive::SetAccessModeAndResetPos(Access mode)
    {
        m_mode = mode;
        m_pos = 0;

        if (IsReadMode())
            (*this) >> m_version;
        else
            (*this) << m_version;
    }

    bool Archive::IsOpen() const
    {
        return m_dataPtr != nullptr;
    }

    void Archive::Close()
    {
        //if (!readMode && !fileName.empty())
        //{
        //    SaveFile(fileName);
        //}
        m_data.clear();
    }

    bool Archive::SaveFile(const std::string filename)
    {
        return filesystem::WriteFile(filename, m_dataPtr, m_pos);
    }

    Archive& Archive::operator<<(char data)
    {
        UnsafeWrite<char>(data);
        return *this;
    }

    Archive& Archive::operator<<(int8_t data)
    {
        UnsafeWrite<int8_t>(data);
        return *this;
    }

    Archive& Archive::operator<<(uint8_t data)
    {
        UnsafeWrite<uint8_t>(data);
        return *this;
    }

    Archive& Archive::operator<<(int32_t data)
    {
        UnsafeWrite<int32_t>(data);
        return *this;
    }

    Archive& Archive::operator<<(uint32_t data)
    {
        UnsafeWrite<uint32_t>(data);
        return *this;
    }

    Archive& Archive::operator<<(int64_t data)
    {
        UnsafeWrite<int64_t>(data);
        return *this;
    }

    Archive& Archive::operator<<(float data)
    {
        UnsafeWrite<float>(data);
        return *this;
    }

    Archive& Archive::operator<<(uint64_t data)
    {
        UnsafeWrite<uint64_t>(data);
        return *this;
    }

    Archive& Archive::operator<<(const std::string& data)
    {
        (*this) << data.length();
        for (const auto& x : data)
        {
            (*this) << x;
        }
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT3& data)
    {
        UnsafeWrite(data);
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT4& data)
    {
        UnsafeWrite(data);
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT4X4& data)
    {
        UnsafeWrite(data);
        return *this;
    }

    Archive& Archive::operator>>(char& data)
    {
        UnsafeRead<char>(data);
        return *this;
    }

    Archive& Archive::operator>>(int8_t& data)
    {
        UnsafeRead<int8_t>(data);
        return *this;
    }

    Archive& Archive::operator>>(uint8_t& data)
    {
        UnsafeRead<uint8_t>(data);
        return *this;
    }

    Archive& Archive::operator>>(int32_t& data)
    {
        UnsafeRead<int32_t>(data);
        return *this;
    }

    Archive& Archive::operator>>(uint32_t& data)
    {
        UnsafeRead<uint32_t>(data);
        return *this;
    }

    Archive& Archive::operator>>(int64_t& data)
    {
        UnsafeRead<int64_t>(data);
        return *this;
    }

    Archive& Archive::operator>>(uint64_t& data)
    {
        UnsafeRead<uint64_t>(data);
        return *this;
    }

    Archive& Archive::operator>>(float& data)
    {
        UnsafeRead<float>(data);
        return *this;
    }

    Archive& Archive::operator>>(std::string& data)
    {
        size_t len;
        (*this) >> len;
        data.resize(len);
        for (size_t i = 0; i < len; i++)
        {
            (*this) >> data[i];
        }
        
        return *this;
    }

    Archive& Archive::operator>>(XMFLOAT3& data)
    {
        UnsafeRead(data);
        return *this;
    }

    Archive& Archive::operator>>(XMFLOAT4& data)
    {
        UnsafeRead(data);
        return *this;
    }

    Archive& Archive::operator>>(XMFLOAT4X4& data)
    {
        UnsafeRead(data);
        return *this;
    }
}