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

    Archive& Archive::operator<<(char value)
    {
        Write(&value, 1);
        return *this;
    }

    Archive& Archive::operator<<(uint8_t value)
    {
        Write(&value, 1);
        return *this;
    }

    Archive& Archive::operator<<(int32_t value)
    {
        Write(&value, 4);
        return *this;
    }

    Archive& Archive::operator<<(uint32_t value)
    {
        Write(&value, 4);
        return *this;
    }

    Archive& Archive::operator<<(int64_t value)
    {
        Write(&value, 8);
        return *this;
    }

    Archive& Archive::operator<<(uint64_t value)
    {
        Write(&value, 8);
        return *this;
    }

    Archive& Archive::operator<<(float value)
    {
        Write(&value, 4);
        return *this;
    }

    Archive& Archive::operator<<(const std::string& str)
    {
        (*this) << str.length();
        Write((void*)str.data(), str.length());
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT3& value)
    {
        Write((void*)&value, sizeof(value));
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT4& value)
    {
        Write((void*)&value, sizeof(value));
        return *this;
    }

    Archive& Archive::operator<<(const XMFLOAT4X4& value)
    {
        Write((void*)&value, sizeof(value));
        return *this;
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

    const Archive& Archive::operator>>(char& value) const
    {
        Read(&value, 1);
        return *this;
    }

    const Archive& Archive::operator>>(uint8_t& value) const
    {
        Read(&value, 1);
        return *this;
    }

    const Archive& Archive::operator>>(int32_t& value) const
    {
        Read(&value, 4);
        return *this;
    }

    const Archive& Archive::operator>>(uint32_t& value) const
    {
        Read(&value, 4);
        return *this;
    }

    const Archive& Archive::operator>>(int64_t& value) const
    {
        Read(&value, 8);
        return *this;
    }

    const Archive& Archive::operator>>(uint64_t& value) const
    {
        Read(&value, 8);
        return *this;
    }

    const Archive& Archive::operator>>(float& value) const
    {
        Read(&value, 4);
        return *this;
    }

    const Archive& Archive::operator>>(std::string& str) const
    {
        size_t length;
        (*this) >> length;
        str.resize(length);
        Read(str.data(), length);
        return *this;
    }

    const Archive& Archive::operator>>(XMFLOAT3& value) const
    {
        Read(&value, sizeof(value));
        return *this;
    }

    const Archive& Archive::operator>>(XMFLOAT4& value) const
    {
        Read(&value, sizeof(value));
        return *this;
    }

    const Archive& Archive::operator>>(XMFLOAT4X4& value) const
    {
        Read(&value, sizeof(value));
        return *this;
    }
}