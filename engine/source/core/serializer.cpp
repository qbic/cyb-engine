#include "core/logger.h"
#include "core/serializer.h"
#include "core/filesystem.h"

namespace cyb
{
    Archive::Archive()
    {
        InitWrite();
    }

    Archive::Archive(const uint8_t* data, size_t length)
    {
        InitRead(data, length);
    }

    void Archive::InitWrite(size_t initWriteBufferSize)
    {
        readData = nullptr;
        writeBuffer.resize(initWriteBufferSize);
        writeData = writeBuffer.data();
        curSize = 0;
    }

    void Archive::InitRead(const uint8_t* data, size_t length)
    {
        writeData = nullptr;
        readData = data;
        readDataLength = length;
        readCount = 0;
    }

    Archive& Archive::operator<<(char value)
    {
        Write<char>(value);
        return *this;
    }

    Archive& Archive::operator<<(int8_t value)
    {
        Write<int8_t>(value);
        return *this;
    }

    Archive& Archive::operator<<(uint8_t value)
    {
        Write<uint8_t>(value);
        return *this;
    }

    Archive& Archive::operator<<(int32_t value)
    {
        Write<int32_t>(value);
        return *this;
    }

    Archive& Archive::operator<<(uint32_t value)
    {
        Write<uint32_t>(value);
        return *this;
    }

    Archive& Archive::operator<<(int64_t value)
    {
        Write<int64_t>(value);
        return *this;
    }

    Archive& Archive::operator<<(float value)
    {
        Write<float>(value);
        return *this;
    }

    Archive& Archive::operator<<(uint64_t value)
    {
        Write<uint64_t>(value);
        return *this;
    }

    Archive& Archive::operator<<(const std::string& str)
    {
        (*this) << str.length();
        for (const auto& x : str)
        {
            (*this) << x;
        }
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT3& value)
    {
        Write(value);
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT4& value)
    {
        Write(value);
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT4X4& value)
    {
        Write(value);
        return *this;
    }

    Archive& Archive::operator>>(char& value)
    {
        Read<char>(value);
        return *this;
    }

    Archive& Archive::operator>>(int8_t& value)
    {
        Read<int8_t>(value);
        return *this;
    }

    Archive& Archive::operator>>(uint8_t& value)
    {
        Read<uint8_t>(value);
        return *this;
    }

    Archive& Archive::operator>>(int32_t& value)
    {
        Read<int32_t>(value);
        return *this;
    }

    Archive& Archive::operator>>(uint32_t& value)
    {
        Read<uint32_t>(value);
        return *this;
    }

    Archive& Archive::operator>>(int64_t& value)
    {
        Read<int64_t>(value);
        return *this;
    }

    Archive& Archive::operator>>(uint64_t& value)
    {
        Read<uint64_t>(value);
        return *this;
    }

    Archive& Archive::operator>>(float& value)
    {
        Read<float>(value);
        return *this;
    }

    Archive& Archive::operator>>(std::string& str)
    {
        size_t len;
        (*this) >> len;
        str.resize(len);
        for (size_t i = 0; i < len; i++)
        {
            (*this) >> str[i];
        }
        
        return *this;
    }

    Archive& Archive::operator>>(XMFLOAT3& value)
    {
        Read(value);
        return *this;
    }

    Archive& Archive::operator>>(XMFLOAT4& value)
    {
        Read(value);
        return *this;
    }

    Archive& Archive::operator>>(XMFLOAT4X4& value)
    {
        Read(value);
        return *this;
    }
}