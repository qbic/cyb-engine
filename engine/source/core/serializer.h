#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "core/filesystem.h"

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;

namespace cyb {

    constexpr uint64_t ARCHIVE_VERSION = 4;

    class Archive {
    public:
        Archive(const Archive&) = delete;
        Archive(Archive&&) = delete;
        Archive& operator=(const Archive&) = delete;

        // initializes the archive for writing
        Archive(size_t initWriteBufferSize);

        // initializes the archive for reading
        Archive(const uint8_t* data, size_t length);

        void InitRead(const uint8_t* data, size_t length);
        void InitWrite(size_t initWriteBufferSize);

        [[nodiscard]] bool IsReading() const { return m_readData != nullptr; }
        [[nodiscard]] bool IsWriting() const { return m_writeData != nullptr; }

        [[nodiscard]] const uint8_t* GetWriteData() const { return m_writeData; }
        [[nodiscard]] size_t Size() const { return m_curSize; }

        [[nodiscard]] size_t Read(void* data, size_t length) const;
        [[nodiscard]] char ReadChar() const;
        [[nodiscard]] uint8_t ReadByte() const;
        [[nodiscard]] uint32_t ReadInt() const;
        [[nodiscard]] uint64_t ReadLong() const;
        [[nodiscard]] float ReadFloat() const;
        [[nodiscard]] std::string ReadString() const;

        void Write(void* data, size_t length);
        void WriteChar(char value);
        void WriteByte(uint8_t value);
        void WriteInt(uint32_t value);
        void WriteLong(uint64_t value);
        void WriteFloat(float value);
        void WriteString(const std::string& str);

    private:
        std::vector<uint8_t> m_writeBuffer;
        uint8_t* m_writeData = nullptr;
        size_t m_curSize = 0;

        const uint8_t* m_readData = nullptr;
        size_t m_readDataLength = 0;
        mutable size_t m_readCount = 0;
    };

    class Serializer {
    public:
        Serializer(Archive& ar);
        ~Serializer() = default;

        [[nodiscard]] bool IsReading() const { return !m_writing; }
        [[nodiscard]] bool IsWriting() const { return m_writing; }
        [[nodiscard]] uint64_t GetVersion() const { return m_version; }

        void Serialize(char& value) { if (m_writing) { m_archive->WriteChar(value); } else { value = m_archive->ReadChar(); }}
        void Serialize(uint8_t& value) { if (m_writing) { m_archive->WriteByte(value); } else { value = m_archive->ReadByte(); }}
        void Serialize(uint32_t& value) { if (m_writing) { m_archive->WriteInt(value); } else { value = m_archive->ReadInt(); }}
        void Serialize(uint64_t& value) { if (m_writing) { m_archive->WriteLong(value); } else { value = m_archive->ReadLong(); }}
        void Serialize(float& value) { if (m_writing) { m_archive->WriteFloat(value); } else { value = m_archive->ReadFloat(); }}
        void Serialize(std::string& value) { if (m_writing) { m_archive->WriteString(value); } else { value = m_archive->ReadString(); }}
        void Serialize(XMFLOAT3& value);
        void Serialize(XMFLOAT4& value);
        void Serialize(XMFLOAT4X4& value);

        template <typename T>
        void Serialize(std::vector<T>& vec) {
            size_t numElements = vec.size();
            Serialize(numElements);

            const size_t numBytes = numElements * sizeof(T);
            if (m_writing) {
                m_archive->Write(vec.data(), numBytes);
            } else {
                vec.resize(numElements);
                size_t bytesRead = m_archive->Read(vec.data(), numBytes);
                assert(bytesRead == numBytes);
            }
        }

    private:
        Archive* m_archive;
        bool m_writing;
        uint64_t m_version;
    };

    template <typename T>
    bool SerializeFromFile(const std::string filename, T& serializeable) {
        std::vector<uint8_t> buffer;
        if (!filesystem::ReadFile(filename, buffer))
            return false;

        Archive archive(buffer.data(), buffer.size());
        Serializer ser(archive);
        serializeable.Serialize(ser);
        return true;
    }

    template <typename T>
    bool SerializeToFile(const std::string& filename, T& serializeable) {
        Archive ar(4 * 1024);  // pre-allocate 4k write buffer
        Serializer ser(ar);
        serializeable.Serialize(ser);

        return filesystem::WriteFile(filename, ar.GetWriteData(), ar.Size());
    }
}