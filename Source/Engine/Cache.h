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

#ifndef ENGINE_CACHE_H
#define ENGINE_CACHE_H

#include "Util/Types.h"

namespace Cache
{
    using AdditionalHeaderData = std::optional<std::vector<u8>>;

    enum class AssetType : u8
    {
        Texture
    };

    enum class CompressionType : u8
    {
        None,
        LZ4,
        ZSTD
    };

    struct Header
    {
        [[nodiscard]] bool Validate() const;

        u32                    magic                    = 0;
        u8                     version                  = 0;
        Cache::AssetType       assetType                = AssetType::Texture;
        Cache::CompressionType compressionType          = CompressionType::LZ4;
        u8                     headerSize               = 0;
        u64                    additionalHeaderDataSize = 0;
        u64                    compressedDataSize       = 0;
        u64                    uncompressedDataSize     = 0;
        u64                    hash                     = 0;
    };

    struct TextureHeader
    {
        [[nodiscard]] bool Validate() const;

        u32             width           = 0;
        u32             height          = 0;
        u32             mipLevels       = 0;
        u32             arrayLayers     = 0;
        u32             faceCount       = 0;
        VkFormat        format          = VK_FORMAT_UNDEFINED;
        bool            generateMipmaps = false;
        u8              padding[3]      = {};
    };

    using AssetHeader = std::variant<TextureHeader>;

    struct Entry
    {
        std::string                 cacheFile             = "Null/Cache";
        Cache::AssetType            assetType             = AssetType::Texture;
        Cache::CompressionType      compressionType       = CompressionType::LZ4;
        Cache::AssetHeader          assetHeader           = {};
        u64                         hash                  = 0;
        Cache::AdditionalHeaderData additionalHeaderData  = std::nullopt;
        std::vector<u8>             data                  = {};
    };

    struct Query
    {
        std::string      cachedFile = "Null/Cache";
        Cache::AssetType assetType  = AssetType::Texture;
        u64              hash       = 0;
    };

    struct Hit
    {
        Cache::AssetHeader          assetHeader          = {};
        Cache::AdditionalHeaderData additionalHeaderData = std::nullopt;
        std::vector<u8>             data                 = {};
    };

    [[nodiscard]] std::vector<u8>               GenerateTextureOffsetTable(const std::span<const VkDeviceSize> offsets);
    [[nodiscard]] std::span<const VkDeviceSize> ExtractTextureOffsetTable(const std::span<const u8> offsetTable);

    void InsertIntoCache(const Cache::Entry& entry);

    // Checks and validates cached file
    [[nodiscard]] bool IsInCache(const Cache::Query& query);

    // UNSAFE: Loads data from cached file WITHOUT DOING ANY VALIDATION, call IsInCache before calling this!
    [[nodiscard]] Cache::Hit GetFromCache(const std::string_view file);

    namespace Detail
    {
        [[nodiscard]] u8 GetAssetHeaderSize(const Cache::AssetType assetType);

        [[nodiscard]] std::vector<u8> CompressData(Cache::CompressionType compressionType, const std::vector<u8>& uncompressedData);

        void StoreHeader(std::ofstream& bin, const Cache::Header& header);
        void StoreAssetHeader(std::ofstream& bin, const Cache::AssetHeader& assetHeader);
        void StoreAdditionalHeaderData(std::ofstream& bin, const Cache::AdditionalHeaderData& additionalHeaderData);
        void StoreData(std::ofstream& bin, Cache::CompressionType compressionType, const std::vector<u8>& uncompressedData, const std::vector<u8>& compressedData);

        [[nodiscard]] Cache::Header LoadHeader(std::ifstream& bin);
        [[nodiscard]] Cache::AssetHeader LoadAssetHeader(std::ifstream& bin, Cache::AssetType assetType);
        [[nodiscard]] Cache::AdditionalHeaderData LoadAdditionalHeaderData(std::ifstream& bin, u64 additionalHeaderDataSize);
        [[nodiscard]] std::vector<u8> LoadAndDecompressData(std::ifstream& bin, const Cache::Header& header);

        void Invalidate(std::ifstream& bin, const std::string_view file);
    }
}

#endif