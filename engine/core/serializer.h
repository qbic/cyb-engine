#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "core/logger.h"
#include "core/timer.h"
#include "core/filesystem.h"

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMFLOAT4X4;

namespace cyb
{
    constexpr uint32_t ARCHIVE_VERSION = 5;

    class Archive
    {
    public:
        Archive(const Archive&) = delete;
        Archive(Archive&&) = delete;
        Archive& operator=(const Archive&) = delete;

        // passing a nullptr to data will initialize the archive for writing
        Archive(const uint8_t* data, size_t length);

        [[nodiscard]] bool IsReading() const;
        [[nodiscard]] bool IsWriting() const;

        [[nodiscard]] const uint8_t* GetReadData() const;
        [[nodiscard]] const uint8_t* GetReadDataAtCurrentPosition() const;
        [[nodiscard]] const uint8_t* GetWriteData() const;
        [[nodiscard]] size_t Size() const;

        [[nodiscard]] size_t Read(void* data, size_t length) const;
        void Read(char& value) const;
        void Read(uint8_t& value) const;
        void Read(uint32_t& value) const;
        void Read(uint64_t& value) const;
        void Read(float& value) const;
        void Read(std::string& value) const;

        void Write(const void* data, size_t length);
        void Write(char value);
        void Write(uint8_t value);
        void Write(uint32_t value);
        void Write(uint64_t value);
        void Write(float value);
        void Write(const std::string& str);

    private:
        std::vector<uint8_t> m_writeBuffer;
        uint8_t* m_writeData = nullptr;
        const uint8_t* m_readData = nullptr;
        size_t m_readDataLength = 0;
        mutable size_t m_position = 0;
    };

    // header data for cyb scene data (.csd) file
    constexpr uint32_t CSD_MAGIC = ('.') | ('c' << 8) | ('s' << 16) | ('d' << 24);
    struct CSD_Header
    {
        uint32_t version;
        union Info
        {
            struct
            {
                uint32_t compressed : 1;
                uint32_t reserved : 31;
            } bits;
            uint32_t raw;
        } info;
        uint32_t decompressedSize;
        uint32_t magic;
        uint64_t reserved[2];
    };

    class Serializer
    {
    public:
        Serializer(Archive& ar, int32_t version = ARCHIVE_VERSION);
        ~Serializer() = default;

        [[nodiscard]] bool IsReading() const;
        [[nodiscard]] bool IsWriting() const;
        [[nodiscard]] uint32_t GetVersion() const;

        void Serialize(char& value);
        void Serialize(uint8_t& value);
        void Serialize(uint32_t& value);
        void Serialize(uint64_t& value);
        void Serialize(float& value);
        void Serialize(std::string& value);
        void Serialize(XMFLOAT3& value);
        void Serialize(XMFLOAT4& value);
        void Serialize(XMFLOAT4X4& value);

        template <typename T>
        void Serialize(std::vector<T>& vec)
        {
            size_t numElements = vec.size();
            Serialize(numElements);

            const size_t numBytes = numElements * sizeof(T);
            if (IsWriting())
            {
                m_archive->Write(vec.data(), numBytes);
            }
            else
            {
                vec.resize(numElements);
                size_t bytesRead = m_archive->Read(vec.data(), numBytes);
                assert(bytesRead == numBytes);
            }
        }

    private:
        Archive* m_archive;
        uint32_t m_version;
    };

    void Decompress(const uint8_t* source, size_t sourceSize, std::vector<uint8_t>& dest, uint64_t decompressedSize);
    void Compress(const uint8_t* source, size_t sourceSize, std::vector<uint8_t>& dest);

    template <typename T>
    bool SerializeFromFile(const std::string filename, T& serializeable)
    {
        Timer timer;
        std::vector<uint8_t> buffer;
        if (!filesystem::ReadFile(filename, buffer))
            return false;

        Archive archive(buffer.data(), buffer.size());
        CSD_Header header = {};
        size_t ret = archive.Read(&header, sizeof(CSD_Header));
        assert(ret == sizeof(CSD_Header));

        if (header.magic != CSD_MAGIC)
        {
            CYB_ERROR("Bad file magic on file {}", filename);
            return false;
        }

        if (header.info.bits.compressed)
        {
            std::vector<uint8_t> decompressedData;
            Decompress(
                archive.GetReadData() + sizeof(CSD_Header),
                archive.Size() - sizeof(CSD_Header),
                decompressedData, 
                header.decompressedSize
            );
            
            if (decompressedData.empty())
            {
                CYB_ERROR("Failed to import {}, data decompression failed", filename);
                return false;
            }

            Archive decompressedArchive(decompressedData.data(), decompressedData.size());
            Serializer ser(decompressedArchive, header.version);
            serializeable.Serialize(ser);
        }
        else
        {
            Serializer ser(archive, header.version);
            serializeable.Serialize(ser);
        }

        CYB_INFO("Imported scene from file {} in {:.2f}ms", filename, timer.ElapsedMilliseconds());
        return true;
    }

    template <typename T>
    bool SerializeToFile(const std::string& filename, T& serializeable, bool useCompression)
    {
        // serialize scene data
        Archive sceneDataArchive(nullptr, 0);
        Serializer ser(sceneDataArchive);
        serializeable.Serialize(ser);

        // create archive and write the header
        Archive archive(nullptr, 0);
        CSD_Header header = {};
        header.version = ARCHIVE_VERSION;
        header.magic = CSD_MAGIC;
        header.info.bits.compressed = useCompression ? 1 : 0;
        header.decompressedSize = useCompression ? sceneDataArchive.Size() : 0;
        archive.Write(&header, sizeof(CSD_Header));
        
        if (header.info.bits.compressed)
        {
            std::vector<uint8_t> compressedData;
            Compress(
                sceneDataArchive.GetWriteData(),
                sceneDataArchive.Size(),
                compressedData
            );

            if (compressedData.size() == 0)
            {
                CYB_ERROR("Failed to write file {}, data compression failed", filename);
                return false;
            }

            archive.Write(compressedData.data(), compressedData.size());
        }
        else
        {
            archive.Write((void*)sceneDataArchive.GetWriteData(), sceneDataArchive.Size());
        }

        return filesystem::WriteFile(filename, archive.GetWriteData(), archive.Size());
    }
}