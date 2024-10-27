#include "core/serializer.h"

namespace cyb {

    Archive::Archive(size_t initWriteBufferSize) {
        InitWrite(initWriteBufferSize);
    }

    Archive::Archive(const uint8_t* data, size_t length) {
        InitRead(data, length);
    }

    void Archive::InitRead(const uint8_t* data, size_t length) {
        m_writeData = nullptr;
        m_readData = data;
        m_readDataLength = length;
        m_readCount = 0;
    }

    void Archive::InitWrite(size_t initWriteBufferSize) {
        m_readData = nullptr;
        m_writeBuffer.resize(initWriteBufferSize);
        m_writeData = m_writeBuffer.data();
        m_curSize = 0;
    }

    size_t Archive::Read(void* data, size_t length) const {
        assert(IsReading());
        assert(m_readData != nullptr);

        // check for overflow
        if (m_readCount + length > m_readDataLength)
            length = m_readDataLength - m_readCount;

        memcpy(data, m_readData + m_readCount, length);
        m_readCount += length;
        return length;
    }

#define READ_TYPE_IMPL(type, length) {          \
    type value;                                 \
    size_t bytesRead = Read(&value, length);    \
    assert(bytesRead == length);                \
    return value;                               \
}

    char Archive::ReadChar() const {
        READ_TYPE_IMPL(char, 1);
    }

    uint8_t Archive::ReadByte() const {
        READ_TYPE_IMPL(uint8_t, 1);
    }

    uint32_t Archive::ReadInt() const {
        READ_TYPE_IMPL(uint32_t, 4);
    }

    uint64_t Archive::ReadLong() const {
        READ_TYPE_IMPL(uint64_t, 8);
    }

    float Archive::ReadFloat() const {
        READ_TYPE_IMPL(float, 4);
    }

    std::string Archive::ReadString() const {
        size_t length = 0;
        for (size_t i = m_readCount; i < m_readDataLength; i++) {
            if (m_readData[i] == 0)
                break;
            length++;
        }

        std::string str((const char *)m_readData + m_readCount, length + 1);
        m_readCount += length + 1;
        return str;
    }

    void Archive::Write(void* data, size_t length) {
        assert(IsWriting());
        assert(m_writeData != nullptr);

        // dynamically stretch write buffer to fit new writes
        while (m_curSize + length > m_writeBuffer.size()) {
            m_writeBuffer.resize((m_curSize + length) * 2);
            m_writeData = m_writeBuffer.data();
        }

        memcpy(m_writeData + m_curSize, data, length);
        m_curSize += length;
    }

    void Archive::WriteChar(char value) {
        Write(&value, 1);
    }

    void Archive::WriteByte(uint8_t value) {
        Write(&value, 1);
    }

    void Archive::WriteInt(uint32_t value) {
        Write(&value, 4);
    }

    void Archive::WriteLong(uint64_t value) {
        Write(&value, 8);
    }

    void Archive::WriteFloat(float value) {
        Write(&value, 4);
    }

    void Archive::WriteString(const std::string& str) {
        Write((void*)str.data(), str.length());
    }

    Serializer::Serializer(Archive& ar) :
        m_archive(&ar) {
        m_writing = m_archive->IsWriting();

        m_version = ARCHIVE_VERSION;
        Serialize(m_version);
    }

    void Serializer::Serialize(XMFLOAT3& value) { 
        if (m_writing) {
            m_archive->Write(&value, sizeof(XMFLOAT3));
        } else {
            size_t bytesRead = m_archive->Read(&value, sizeof(XMFLOAT3));
            assert(bytesRead == sizeof(XMFLOAT3));
        }
    }

    void Serializer::Serialize(XMFLOAT4& value) {
        if (m_writing) {
            m_archive->Write(&value, sizeof(XMFLOAT4));
        }
        else {
            size_t bytesRead = m_archive->Read(&value, sizeof(XMFLOAT4));
            assert(bytesRead == sizeof(XMFLOAT4));
        }
    }

    void Serializer::Serialize(XMFLOAT4X4& value) {
        if (m_writing) {
            m_archive->Write(&value, sizeof(XMFLOAT4X4));
        }
        else {
            size_t bytesRead = m_archive->Read(&value, sizeof(XMFLOAT4X4));
            assert(bytesRead == sizeof(XMFLOAT4X4));
        }
    }
}