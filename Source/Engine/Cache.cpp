/*
 * Copyright (c) 2023 - 2026 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Cache.h"

#include "IBL/BRDF.h"
#include "Renderer/IBL/IBLMaps.h"
#include "Util/Files.h"
#include "Util/Hash.h"
#include "Util/Log.h"
#include "Util/Span.h"
#include "Util/Visitor.h"

namespace Cache
{
    constexpr auto CACHE_DIRECTORY      = "Cache/";
    constexpr u32  CACHE_HEADER_MAGIC   = 0x4B4F4F43;
    constexpr u8   CACHE_HEADER_VERSION = 6;

    void InsertIntoCache(const Cache::Entry& entry)
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", entry.cacheFile.c_str());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(entry.cacheFile);

        auto bin = std::ofstream(path, std::ios::binary | std::ios::out);

        if (!bin.is_open())
        {
            Logger::Error("Failed to create binary {}!\n", path);
        }

        const auto compressedData = Detail::CompressData(entry.compressionType, entry.data);

        Detail::StoreHeader(bin, Cache::Header
        {
            .magic                = CACHE_HEADER_MAGIC,
            .version              = CACHE_HEADER_VERSION,
            .assetType            = entry.assetType,
            .pad0                 = 0,
            .headerSize           = Detail::GetAssetHeaderSize(entry.assetType),
            .compressedDataSize   = compressedData.size(),
            .uncompressedDataSize = entry.data.size(),
            .hash                 = entry.hash
        });

        Detail::StoreAssetHeader(bin, entry.assetHeader);

        Detail::StoreTextureOffsetTable(bin, entry.textureOffsetTable);

        Detail::StoreData
        (
            bin,
            entry.compressionType,
            entry.data,
            compressedData
        );
    }

    bool IsInCache(const Cache::Query& query)
    {
        // NOTE: I could verify the compression type per cached file.
        // But I don't really see the point in a cached file changing its compression type.
        // So that case will be undefined behavior for now.
        // This can be a security issue, and it might be worth forcing the query to specify the compression type.

        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", query.cachedFile.c_str());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(query.cachedFile);
        
        if (!Files::Exists(path))
        {
            return false;
        }

        auto bin = std::ifstream(path, std::ios::binary | std::ios::in);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load binary {}!\n", path);
        }

        const auto header = Detail::LoadHeader(bin);

        if (!header.Validate())
        {
            Detail::Invalidate(bin, path);

            return false;
        }

        if (header.assetType != query.assetType)
        {
            Detail::Invalidate(bin, path);

            return false;
        }

        if (header.headerSize != Detail::GetAssetHeaderSize(header.assetType))
        {
            Detail::Invalidate(bin, path);

            return false;
        }

        if (header.hash != query.hash)
        {
            Detail::Invalidate(bin, path);

            return false;
        }

        const auto assetHeader = Detail::LoadAssetHeader(bin, header.assetType);

        const auto [validationResult, textureOffsetTableSize] = std::visit(Util::Visitor{
           [] (const Cache::TextureHeader& textureHeader)
           {
               return std::make_pair(textureHeader.Validate(), textureHeader.offsetTableSize);
           }
        }, assetHeader);

        if (!validationResult)
        {
            Detail::Invalidate(bin, path);

            return false;
        }

        const usize expectedDataSize = header.compressionType == CompressionType::None ?
                                       header.uncompressedDataSize :
                                       header.compressedDataSize;

        const usize expectedFileSize = sizeof(Cache::Header) +
                                       header.headerSize +
                                       textureOffsetTableSize +
                                       expectedDataSize;

        if (Files::GetSize(path) != expectedFileSize)
        {
            Detail::Invalidate(bin, path);

            return false;
        }

        return true;
    }

    Cache::Entry GetFromCache(const std::string_view file)
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", file.data());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(file);

        auto bin = std::ifstream(path, std::ios::binary | std::ios::in);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load binary {}!\n", path);
        }

        const auto header = Detail::LoadHeader(bin);

        const auto [assetHeader, textureOffsetTable] = Detail::LoadAssetHeaderAndTextureOffsetTable(bin, header.assetType);

        const auto data = Detail::LoadAndDecompressData(bin, header);

        return Cache::Entry
        {
            .cacheFile          = file.data(),
            .assetType          = header.assetType,
            .compressionType    = header.compressionType,
            .assetHeader        = assetHeader,
            .hash               = header.hash,
            .textureOffsetTable = textureOffsetTable,
            .data               = std::move(data)
        };
    }
    
    bool Header::Validate() const
    {
        return magic == CACHE_HEADER_MAGIC &&
               version == CACHE_HEADER_VERSION &&
               headerSize != 0 &&
               uncompressedDataSize != 0;
    }

    bool TextureHeader::Validate() const
    {
        return width != 0 &&
               height != 0 &&
               mipLevels != 0 &&
               arrayLayers != 0 &&
               faceCount != 0 &&
               format != VK_FORMAT_UNDEFINED &&
               (offsetTableSize != 0 && offsetTableSize % sizeof(VkDeviceSize) == 0);
    }

    std::vector<u8> GenerateTextureOffsetTable(const std::span<const VkDeviceSize> offsets)
    {
        const auto asBytes = Util::ToBytes(offsets);

        return std::vector(asBytes.begin(), asBytes.end());
    }

    std::span<const VkDeviceSize> ExtractTextureOffsetTable(const std::span<const u8> offsetTable)
    {
        ENGINE_ASSERT(offsetTable.size() % sizeof(VkDeviceSize) == 0);

        return {reinterpret_cast<const VkDeviceSize*>(offsetTable.data()), offsetTable.size_bytes() / sizeof(VkDeviceSize)};
    }

    namespace Detail
    {
        usize GetAssetHeaderSize(const Cache::AssetType assetType)
        {
            switch (assetType)
            {
            case AssetType::Texture:
                return sizeof(Cache::TextureHeader);

            default:
                Logger::Error("{}\n", "Invalid asset type!");
            };
        }

        std::vector<u8> CompressData(Cache::CompressionType compressionType, const std::vector<u8>& uncompressedData)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            switch (compressionType)
            {
            case CompressionType::None:
                return {};

            case CompressionType::LZ4:
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("LZ4HC Compression");
                #endif

                usize compressedDataSize = static_cast<usize>(LZ4_compressBound(static_cast<s32>(uncompressedData.size())));

                auto compressedData = std::vector<u8>(compressedDataSize);

                compressedDataSize = static_cast<usize>(LZ4_compress_HC
                (
                    reinterpret_cast<const char*>(uncompressedData.data()),
                    reinterpret_cast<char*>(compressedData.data()),
                    static_cast<s32>(uncompressedData.size()),
                    static_cast<s32>(compressedData.size()),
                    LZ4HC_CLEVEL_DEFAULT
                ));

                if (compressedDataSize == 0)
                {
                    Logger::Error("{}\n", "Failed to compress data!");
                }

                compressedData.resize(compressedDataSize);

                return compressedData;
            }

            default:
                Logger::Error("{}\n", "Unknown compression type!");
            }
        }

        void StoreHeader(std::ofstream& bin, const Cache::Header& header)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            bin.write(reinterpret_cast<const char*>(&header), sizeof(Cache::Header));
        }

        void StoreAssetHeader(std::ofstream& bin, const Cache::AssetHeader& assetHeader)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            std::visit(Util::Visitor{
                [&bin] (const Cache::TextureHeader& textureHeader)
                {
                    bin.write(reinterpret_cast<const char*>(&textureHeader), sizeof(Cache::TextureHeader));
                }
            }, assetHeader);
        }

        void StoreTextureOffsetTable(std::ofstream& bin, const Cache::TextureOffsetTable& textureOffsetTable)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            if (!textureOffsetTable.has_value())
            {
                return;
            }

            bin.write(reinterpret_cast<const char*>(textureOffsetTable->data()), static_cast<std::streamsize>(textureOffsetTable->size()));
        }

        void StoreData
        (
            std::ofstream& bin,
            Cache::CompressionType compressionType,
            const std::vector<u8>& uncompressedData,
            const std::vector<u8>& compressedData
        )
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            const char* data = compressionType == CompressionType::None ?
                               reinterpret_cast<const char*>(uncompressedData.data()) :
                               reinterpret_cast<const char*>(compressedData.data());

            const std::streamsize dataSize = compressionType == CompressionType::None ?
                                             static_cast<std::streamsize>(uncompressedData.size()) :
                                             static_cast<std::streamsize>(compressedData.size());

            bin.write(data, dataSize);
        }

        Cache::Header LoadHeader(std::ifstream& bin)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            Cache::Header header = {};

            bin.read(reinterpret_cast<char*>(&header), sizeof(Cache::Header));

            return header;
        }

        Cache::AssetHeader LoadAssetHeader(std::ifstream& bin, Cache::AssetType assetType)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            Cache::AssetHeader assetHeader = {};

            switch (assetType)
            {
            case AssetType::Texture:
            {
                Cache::TextureHeader textureHeader = {};

                bin.read(reinterpret_cast<char*>(&textureHeader), sizeof(Cache::TextureHeader));

                assetHeader = textureHeader;

                break;
            }

            default:
                Logger::Error("{}\n", "Unknown cached asset type!");
            }

            return assetHeader;
        }

        std::pair<Cache::AssetHeader, Cache::TextureOffsetTable>
        LoadAssetHeaderAndTextureOffsetTable(std::ifstream& bin, Cache::AssetType assetType)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            const auto assetHeader = LoadAssetHeader(bin, assetType);

            Cache::TextureOffsetTable textureOffsetTable = std::nullopt;

            if (assetType == AssetType::Texture)
            {
                const auto textureHeader = std::get<Cache::TextureHeader>(assetHeader);

                textureOffsetTable = std::vector<u8>(textureHeader.offsetTableSize);

                bin.read(reinterpret_cast<char*>(textureOffsetTable->data()), static_cast<std::streamsize>(textureOffsetTable->size()));
            }

            return std::make_pair(assetHeader, textureOffsetTable);
        }

        std::vector<u8> LoadAndDecompressData(std::ifstream& bin, const Cache::Header& header)
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            auto data = std::vector<u8>(header.uncompressedDataSize);

            switch (header.compressionType)
            {
            case CompressionType::None:
                bin.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
                break;

            case CompressionType::LZ4:
            {
                auto compressedData = std::vector<u8>(header.compressedDataSize);

                bin.read(reinterpret_cast<char*>(compressedData.data()), static_cast<std::streamsize>(compressedData.size()));

                const auto uncompressedSizeSigned = static_cast<ssize>(LZ4_decompress_safe
                (
                    reinterpret_cast<const char*>(compressedData.data()),
                    reinterpret_cast<char*>(data.data()),
                    static_cast<s32>(compressedData.size()),
                    static_cast<s32>(data.size())
                ));

                const auto uncompressedSize = static_cast<usize>(uncompressedSizeSigned);

                if (uncompressedSizeSigned <= 0 || uncompressedSize != header.uncompressedDataSize)
                {
                    Logger::Error("{}\n", "Failed to decompress data!");;
                }

                break;
            }

            default:
                Logger::Error("{}\n", "Unknown compression type!");
            }

            return data;
        }

        void Invalidate(std::ifstream& bin, const std::string_view file)
        {
            bin.close();

            if (!Files::Remove(file))
            {
                Logger::Warning("Failed to remove cached file! [File={}]\n", file);
            }
        }
    }
}