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

        // Initializes the archive for writing
        Archive();

        // Initializes the archive for reading
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

        Archive& operator<<(const std::string& str);
        Archive& operator<<(const XMFLOAT3& value);
        Archive& operator<<(const XMFLOAT4& value);
        Archive& operator<<(const XMFLOAT4X4& value);

        template <typename T>
        Archive& operator<<(std::vector<T>& data)
        {
            size_t count = data.size();
            Write(&count, 8);
            Write(data.data(), count * sizeof(T));
            return *this;
        }

        //=============================================================
        //  Read Operations
        //=============================================================

        size_t Read(void* data, size_t length) const;
        char ReadChar() const;
        uint8_t ReadByte() const;
        uint32_t ReadLong() const;
        uint64_t ReadLongLong() const;
        float ReadFloat() const;

        const Archive& operator>>(std::string& str) const;
        const Archive& operator>>(XMFLOAT3& value) const;
        const Archive& operator>>(XMFLOAT4& value) const;
        const Archive& operator>>(XMFLOAT4X4& value) const;

        template <typename T>
        Archive& operator>>(std::vector<T>& data)
        {
            size_t count = data.size();
            Read(&count, 8);
            data.resize(count);
            Read(data.data(), count * sizeof(T));
            return *this;
        }

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
        [[nodiscard]] Archive& GetArchive() { return *archive; }

        void Serialize(char& value) { if (writing) { archive->WriteChar(value); } else { value = archive->ReadChar(); }}
        void Serialize(uint8_t& value) { if (writing) { archive->WriteByte(value); } else { value = archive->ReadByte(); }}
        void Serialize(uint32_t& value) { if (writing) { archive->WriteLong(value); } else { value = archive->ReadLong(); }}
        void Serialize(uint64_t& value) { if (writing) { archive->WriteLongLong(value); } else { value = archive->ReadLongLong(); }}
        void Serialize(float& value) { if (writing) { archive->WriteFloat(value); } else { value = archive->ReadFloat(); }}

        template <typename T>
        void Serialize(T& value)
        {
            if (writing)
            {
                *archive << value;
            }
            else
            {
                *archive >> value;
            }
        }

    private:
        Archive* archive;
        bool writing;
    };
}