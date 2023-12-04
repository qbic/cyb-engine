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

        Archive& operator<<(char value);
        Archive& operator<<(int8_t value);
        Archive& operator<<(uint8_t value);
        Archive& operator<<(int32_t value);
        Archive& operator<<(uint32_t value);
        Archive& operator<<(int64_t value);
        Archive& operator<<(uint64_t value);
        Archive& operator<<(float value);
        Archive& operator<<(const std::string& str);
        Archive& operator<<(const XMFLOAT3& value);
        Archive& operator<<(const XMFLOAT4& value);
        Archive& operator<<(const XMFLOAT4X4& value);

        template <typename T>
        Archive& operator<<(std::vector<T>& data)
        {
            Write<size_t>(data.size());
            for (const T& x : data)
            {
                *(this) << x;
            }
            return *this;
        }

        //=============================================================
        //  Read Operations
        //=============================================================

        Archive& operator>>(char& value);
        Archive& operator>>(int8_t& value);
        Archive& operator>>(uint8_t& value);
        Archive& operator>>(int32_t& value);
        Archive& operator>>(uint32_t& value);
        Archive& operator>>(int64_t& value);
        Archive& operator>>(uint64_t& value);
        Archive& operator>>(float& value);
        Archive& operator>>(std::string& str);
        Archive& operator>>(XMFLOAT3& value);
        Archive& operator>>(XMFLOAT4& value);
        Archive& operator>>(XMFLOAT4X4& value);

        template <typename T>
        Archive& operator>>(std::vector<T>& data)
        {
            size_t count;
            Read(count);
            data.resize(count);
            for (size_t i = 0; i < count; ++i)
            {
                *(this) >> data[i];
            }
            return *this;
        }

    private:
        // Write data to archive at current position
        template <typename T>
        inline void Write(const T& data)
        {
            assert(IsWriting());
            assert(writeData != nullptr);

            // Dynamically stretch the write buffer
            size_t length = sizeof(data);
            while (curSize + length > writeBuffer.size())
            {
                writeBuffer.resize((curSize + length) * 2);
                writeData = writeBuffer.data();
            }

            *(T*)(writeBuffer.data() + curSize) = data;
            curSize += length;
        }

        // Read data from archive as current position
        template <typename T>
        inline void Read(T& data)
        {
            assert(IsReading());
            assert(readData != nullptr);

            data = *(const T*)(readData + readCount);
            readCount += (size_t)(sizeof(data));
        }

    private:
        std::vector<uint8_t> writeBuffer;
        uint8_t* writeData = nullptr;
        size_t curSize = 0;

        const uint8_t* readData = nullptr;
        size_t readDataLength = 0;
        size_t readCount = 0;
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