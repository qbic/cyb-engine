#include "core/serializer.h"

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

    void Archive::Write(void* data, size_t length)
    {
        assert(IsWriting());
        assert(writeData != nullptr);

        // Dynamically stretch the write buffer
        while (curSize + length > writeBuffer.size())
        {
            writeBuffer.resize((curSize + length) * 2);
            writeData = writeBuffer.data();
        }

        memcpy(writeData + curSize, data, length);
        curSize += length;
    }

    void Archive::WriteChar(char value)
    {
        Write(&value, 1);
    }

    void Archive::WriteByte(uint8_t value)
    {
        Write(&value, 1);
    }

    void Archive::WriteLong(uint32_t value)
    {
        Write(&value, 4);
    }

    void Archive::WriteLongLong(uint64_t value)
    {
        Write(&value, 8);
    }

    void Archive::WriteFloat(float value)
    {
        Write(&value, 4);
    }

    void Archive::WriteString(const std::string& str)
    {
        Write((void*)str.data(), str.length());
    }

    void Archive::WriteXMFLOAT3(const XMFLOAT3& value)
    {
        Write((void*)&value, sizeof(value));
    }

    void Archive::WriteXMFLOAT4(const XMFLOAT4& value)
    {
        Write((void*)&value, sizeof(value));
    }

    void Archive::WriteXMFLOAT4X4(const XMFLOAT4X4& value)
    {
        Write((void*)&value, sizeof(value));
    }

    size_t Archive::Read(void* data, size_t length) const
    {
        assert(IsReading());
        assert(readData != nullptr);

        // check for overflow
        if (readCount + length > readDataLength)
        {
            length = readDataLength - readCount;
        }

        memcpy(data, readData + readCount, length);
        readCount += length;
        return length;
    }

#define READ_TYPE_IMPL(type, length)    \
{                                       \
    type value;                         \
    Read(&value, length);               \
    return value;                       \
}

    char Archive::ReadChar() const
    {
        READ_TYPE_IMPL(char, 1);
    }

    uint8_t Archive::ReadByte() const
    {
        READ_TYPE_IMPL(uint8_t, 1);
    }

    uint32_t Archive::ReadLong() const
    {
        READ_TYPE_IMPL(uint32_t, 4);
    }

    uint64_t Archive::ReadLongLong() const
    {
        READ_TYPE_IMPL(uint64_t, 8);
    }

    float Archive::ReadFloat() const
    {
        READ_TYPE_IMPL(float, 4);
    }

    std::string Archive::ReadString() const
    {
        size_t length = 0;
        for (size_t i = readCount; i < readDataLength; i++)
        {
            if (readData[i] == 0)
            {
                break;
            }
            length++;
        }

        std::string str((const char *)readData + readCount, length + 1);
        readCount += length + 1;
        return str;
    }

    XMFLOAT3 Archive::ReadXMFLOAT3() const
    {
        READ_TYPE_IMPL(XMFLOAT3, sizeof(XMFLOAT3));
    }

    XMFLOAT4 Archive::ReadXMFLOAT4() const
    {
        READ_TYPE_IMPL(XMFLOAT4, sizeof(XMFLOAT4));
    }

    XMFLOAT4X4 Archive::ReadXMFLOAT4X4() const
    {
        READ_TYPE_IMPL(XMFLOAT4X4, sizeof(XMFLOAT4X4));
    }
}