#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;

namespace cyb
{
    constexpr size_t ARCHIVE_INIT_SIZE = 128;

    class Archive
    {
    public:
        Archive(const Archive&) = delete;
        Archive(Archive&&) = delete;
        Archive& operator=(const Archive&) = delete;

        // initializes the archive for writing
        Archive();

        // initializes the archive for reading
        Archive(const uint8_t* data, size_t length);

        void InitWrite(size_t initWriteBufferSize = ARCHIVE_INIT_SIZE);
        void InitRead(const uint8_t* data, size_t length);

        [[nodiscard]] bool IsReading() const { return readData != nullptr; }
        [[nodiscard]] bool IsWriting() const { return writeData != nullptr; }

        [[nodiscard]] uint8_t* GetWriteData() const { return writeData; }
        [[nodiscard]] size_t GetSize() const { return curSize; }

        //=============================================================
        //  Write Operations
        //=============================================================

        void Write(void* data, size_t length);
        void WriteChar(char value);
        void WriteByte(uint8_t value);
        void WriteLong(uint32_t value);
        void WriteLongLong(uint64_t value);
        void WriteFloat(float value);
        void WriteString(const std::string& str);
        void WriteXMFLOAT3(const XMFLOAT3& value);
        void WriteXMFLOAT4(const XMFLOAT4& value);
        void WriteXMFLOAT4X4(const XMFLOAT4X4& value);

        //=============================================================
        //  Read Operations
        //=============================================================

        size_t Read(void* data, size_t length) const;
        [[nodiscard]] char ReadChar() const;
        [[nodiscard]] uint8_t ReadByte() const;
        [[nodiscard]] uint32_t ReadLong() const;
        [[nodiscard]] uint64_t ReadLongLong() const;
        [[nodiscard]] float ReadFloat() const;
        [[nodiscard]] std::string ReadString() const;
        [[nodiscard]] XMFLOAT3 ReadXMFLOAT3() const;
        [[nodiscard]] XMFLOAT4 ReadXMFLOAT4() const;
        [[nodiscard]] XMFLOAT4X4 ReadXMFLOAT4X4() const;

    private:
        std::vector<uint8_t> writeBuffer;
        uint8_t* writeData = nullptr;
        size_t curSize = 0;

        const uint8_t* readData = nullptr;
        size_t readDataLength = 0;
        mutable size_t readCount = 0;
    };

    class Serializer
    {
    public:
        Serializer(Archive& ar_) :
            archive(&ar_)
        {
            writing = archive->IsWriting();
        }

        [[nodiscard]] bool IsReading() const { return !writing; }
        [[nodiscard]] bool IsWriting() const { return writing; }

        void Serialize(char& value) { if (writing) { archive->WriteChar(value); } else { value = archive->ReadChar(); }}
        void Serialize(uint8_t& value) { if (writing) { archive->WriteByte(value); } else { value = archive->ReadByte(); }}
        void Serialize(uint32_t& value) { if (writing) { archive->WriteLong(value); } else { value = archive->ReadLong(); }}
        void Serialize(uint64_t& value) { if (writing) { archive->WriteLongLong(value); } else { value = archive->ReadLongLong(); }}
        void Serialize(float& value) { if (writing) { archive->WriteFloat(value); } else { value = archive->ReadFloat(); }}
        void Serialize(std::string& value) { if (writing) { archive->WriteString(value); } else { value = archive->ReadString(); }}
        void Serialize(XMFLOAT3& value) { if (writing) { archive->WriteXMFLOAT3(value); } else { value = archive->ReadXMFLOAT3(); }}
        void Serialize(XMFLOAT4& value) { if (writing) { archive->WriteXMFLOAT4(value); } else { value = archive->ReadXMFLOAT4(); }}
        void Serialize(XMFLOAT4X4& value) { if (writing) { archive->WriteXMFLOAT4X4(value); } else { value = archive->ReadXMFLOAT4X4(); }}

        template <typename T>
        void Serialize(std::vector<T>& vec)
        {
            size_t size = vec.size();
            Serialize(size);
            if (writing)
            {
                archive->Write(vec.data(), size * sizeof(T));
            }
            else
            {
                vec.resize(size);
                archive->Read(vec.data(), size * sizeof(T));
            }
        }

    private:
        Archive* archive;
        bool writing;
    };
}