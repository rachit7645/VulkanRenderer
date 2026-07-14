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
#include "Externals/LZ4.h"
#include "Externals/ZSTD.h"

namespace Cache
{
    constexpr auto CACHE_DIRECTORY      = "Cache/";
    constexpr u32  CACHE_HEADER_MAGIC   = 0x4B4F4F43;
    constexpr u8   CACHE_HEADER_VERSION = 8;

    void InsertIntoCache(const Cache::Entry& entry)
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", entry.cacheFile.c_str());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(entry.cacheFile);

        if (Files::CreateDirectory(CACHE_DIRECTORY))
        {
            Logger::Debug("{}\n", "Created cache directory!");
        };

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
            .compressionType      = entry.compressionType,
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

    Cache::Hit GetFromCache(const std::string_view file)
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

        return Cache::Hit
        {
            .assetHeader        = assetHeader,
            .textureOffsetTable = textureOffsetTable,
            .data               = data
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
        u8 GetAssetHeaderSize(const Cache::AssetType assetType)
        {
            usize headerSize = 0;

            switch (assetType)
            {
            case AssetType::Texture:
                headerSize = sizeof(Cache::TextureHeader);
                break;

            default:
                Logger::Error("{}\n", "Invalid asset type!");
            };

            if (headerSize > std::numeric_limits<u8>::max())
            {
                Logger::Error
                (
                    "Asset header size is too large! [Size={}] [Max={}]",
                    headerSize,
                    std::numeric_limits<u8>::max()
                );
            }

            return static_cast<u8>(headerSize);
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

                auto compressedDataSize = static_cast<usize>(LZ4_compressBound(static_cast<s32>(uncompressedData.size())));

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

            case CompressionType::ZSTD:
            {
                #ifdef ENGINE_PROFILE
                ZoneScopedN("ZSTD Compression");
                #endif

                usize compressedDataSize = ZSTD_compressBound(uncompressedData.size());

                if (ZSTD_isError(compressedDataSize))
                {
                    Logger::Error("Failed to get maximum compressed data size! [Error={}]\n", ZSTD_getErrorName(compressedDataSize));
                }

                auto compressedData = std::vector<u8>(compressedDataSize);

                compressedDataSize = ZSTD_compress
                (
                    compressedData.data(),
                    compressedData.size(),
                    uncompressedData.data(),
                    uncompressedData.size(),
                    ZSTD_CLEVEL_DEFAULT
                );

                if (ZSTD_isError(compressedDataSize))
                {
                    Logger::Error("Failed to compress data! [Error={}]\n", ZSTD_getErrorName(compressedDataSize));
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

            std::vector<u8> uncompressedData = {};

            switch (header.compressionType)
            {
            case CompressionType::None:
            {
                uncompressedData.resize(header.uncompressedDataSize);

                bin.read(reinterpret_cast<char*>(uncompressedData.data()), static_cast<std::streamsize>(uncompressedData.size()));

                break;
            }

            case CompressionType::LZ4:
            {
                uncompressedData.resize(header.uncompressedDataSize);

                auto compressedData = std::vector<u8>(header.compressedDataSize);

                bin.read(reinterpret_cast<char*>(compressedData.data()), static_cast<std::streamsize>(compressedData.size()));

                const auto uncompressedSizeSigned = static_cast<ssize>(LZ4_decompress_safe
                (
                    reinterpret_cast<const char*>(compressedData.data()),
                    reinterpret_cast<char*>(uncompressedData.data()),
                    static_cast<s32>(compressedData.size()),
                    static_cast<s32>(uncompressedData.size())
                ));

                const auto uncompressedSize = static_cast<usize>(uncompressedSizeSigned);

                if (uncompressedSizeSigned <= 0 || uncompressedSize != header.uncompressedDataSize)
                {
                    Logger::Error("{}\n", "Failed to decompress data!");;
                }

                break;
            }

            case CompressionType::ZSTD:
            {
                auto compressedData = std::vector<u8>(header.compressedDataSize);

                bin.read(reinterpret_cast<char*>(compressedData.data()), static_cast<std::streamsize>(compressedData.size()));

                usize frameContentSize = ZSTD_getFrameContentSize(compressedData.data(), compressedData.size());

                if (ZSTD_isError(frameContentSize))
                {
                    if (frameContentSize != ZSTD_CONTENTSIZE_UNKNOWN)
                    {
                        Logger::Warning("{}\n", "Unknown frame content size! We might be screwed chat...");

                        frameContentSize = header.uncompressedDataSize;
                    }
                    else
                    {
                        Logger::Error("Failed to decompress data! [Error={}]\n", ZSTD_getErrorName(frameContentSize));
                    }
                }

                uncompressedData.resize(frameContentSize);

                const usize uncompressedSize = ZSTD_decompress
                (
                    uncompressedData.data(),
                    uncompressedData.size(),
                    compressedData.data(),
                    compressedData.size()
                );

                if (ZSTD_isError(uncompressedSize))
                {
                    Logger::Error("Failed to decompress data! [Error={}]\n", ZSTD_getErrorName(uncompressedSize));
                }

                if (uncompressedSize != frameContentSize || uncompressedSize != header.uncompressedDataSize)
                {
                    Logger::Warning("{}\n", "Mismatched decompressed sizes!");
                }

                uncompressedData.resize(uncompressedSize);

                break;
            }

            default:
                Logger::Error("{}\n", "Unknown compression type!");
            }

            return uncompressedData;
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