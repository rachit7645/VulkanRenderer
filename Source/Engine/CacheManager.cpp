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

#include "CacheManager.h"

#include "IBL/BRDF.h"
#include "Renderer/IBL/IBLMaps.h"
#include "Util/Files.h"
#include "Util/Hash.h"
#include "Util/Log.h"
#include "Util/Visitor.h"

namespace Engine
{
    constexpr auto CACHE_DIRECTORY = "Cache/";

    constexpr u32 CACHE_HEADER_MAGIC   = 0x4B4F4F43;
    constexpr u8  CACHE_HEADER_VERSION = 3;

    CacheManager::CacheManager()
    {
        for (const auto& file : Util::Files::GetFilesInDirectory(CACHE_DIRECTORY))
        {
            m_cacheTable.emplace(file);
        }
    }

    void CacheManager::InsertIntoCache(const Engine::CacheEntry& entry)
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", entry.cacheFile.c_str());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(entry.cacheFile);

        auto bin = std::ofstream(path.data(), std::ios::binary | std::ios::out);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load binary {}!\n", path);
        }

        auto compressedDataSize = static_cast<usize>(LZ4_compressBound(static_cast<s32>(entry.data.size())));

        auto compressedData = std::vector<u8>(compressedDataSize);

        compressedDataSize = static_cast<usize>(LZ4_compress_HC
        (
            reinterpret_cast<const char*>(entry.data.data()),
            reinterpret_cast<char*>(compressedData.data()),
            static_cast<s32>(entry.data.size()),
            static_cast<s32>(compressedData.size()),
            LZ4HC_CLEVEL_DEFAULT
        ));

        if (compressedDataSize == 0)
        {
            Logger::Error("Failed to compress binary {}!\n", path);
        }

        usize headerSize = 0;

        switch (entry.assetType)
        {
        case CachedAssetType::Texture:
            headerSize = sizeof(Engine::CachedTextureHeader);
            break;

        default:
            Logger::Error("{}\n", "Unknown cached asset type!");
        }

        const Engine::CacheHeader header =
        {
            .magic                = CACHE_HEADER_MAGIC,
            .version              = CACHE_HEADER_VERSION,
            .assetType            = entry.assetType,
            .pad0                 = 0,
            .headerSize           = headerSize,
            .compressedDataSize   = compressedDataSize,
            .uncompressedDataSize = entry.data.size(),
            .hash                 = entry.hash
        };

        bin.write(reinterpret_cast<const char*>(&header), sizeof(Engine::CacheHeader));

        std::visit(Util::Visitor{
            [&bin] (const Engine::CachedTextureHeader& textureHeader)
            {
                bin.write(reinterpret_cast<const char*>(&textureHeader), sizeof(Engine::CachedTextureHeader));
            }
        }, entry.assetHeader);

        bin.write(reinterpret_cast<const char*>(compressedData.data()), static_cast<std::streamsize>(compressedDataSize));

        bin.flush();
        bin.close();

        AppendToCacheTable(path);
    }

    bool CacheManager::IsInCache(const Engine::CacheLookup& lookup)
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", lookup.cachedFile.c_str());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(lookup.cachedFile);

        if (!IsInCacheTable(path))
        {
            return false;
        }

        auto bin = std::ifstream(path.data(), std::ios::binary | std::ios::in);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load binary {}!\n", path);
        }

        Engine::CacheHeader header = {};

        bin.read(reinterpret_cast<char*>(&header), sizeof(Engine::CacheHeader));

        if (!header.Validate())
        {
            bin.close();

            InvalidateFromCacheTable(path);

            return false;
        }

        if (header.assetType != lookup.assetType)
        {
            bin.close();

            InvalidateFromCacheTable(path);

            return false;
        }

        switch (header.assetType)
        {
        case CachedAssetType::Texture:
        {
            Engine::CachedTextureHeader textureHeader = {};

            bin.read(reinterpret_cast<char*>(&textureHeader), sizeof(Engine::CachedTextureHeader));

            if (!textureHeader.Validate())
            {
                bin.close();

                InvalidateFromCacheTable(path);

                return false;
            }

            break;
        }

        default:
            Logger::Error("{}\n", "Unknown cached asset type!");
        }

        if (header.hash != lookup.hash)
        {
            bin.close();

            InvalidateFromCacheTable(path);

            return false;
        }

        return true;
    }

    Engine::CacheEntry CacheManager::GetFromCache(const std::string_view file) const
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", file.data());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(file);

        auto bin = std::ifstream(path.data(), std::ios::binary | std::ios::in);

        if (!bin.is_open())
        {
            Logger::Error("Failed to load binary {}!\n", path);
        }

        Engine::CacheHeader header = {};

        bin.read(reinterpret_cast<char*>(&header), sizeof(Engine::CacheHeader));

        Engine::CachedAssetHeader assetHeader = {};

        switch (header.assetType)
        {
        case CachedAssetType::Texture:
        {
            Engine::CachedTextureHeader textureHeader = {};

            bin.read(reinterpret_cast<char*>(&textureHeader), sizeof(Engine::CachedTextureHeader));

            assetHeader = textureHeader;

            break;
        }

        default:
            Logger::Error("{}\n", "Unknown cached asset type!");
        }

        auto compressedData = std::vector<u8>(header.compressedDataSize);

        bin.read(reinterpret_cast<char*>(compressedData.data()), static_cast<std::streamsize>(compressedData.size()));

        auto data = std::vector<u8>(header.uncompressedDataSize);

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
            Logger::Error("Failed to decompress data! [File={}]\n", file);
        }

        return Engine::CacheEntry
        {
            .cacheFile   = file.data(),
            .assetType   = header.assetType,
            .assetHeader = assetHeader,
            .hash        = header.hash,
            .data        = std::move(data)
        };
    }

    void CacheManager::AppendToCacheTable(const std::string_view cacheFile)
    {
        const std::scoped_lock lock{m_mutex};

        m_cacheTable.emplace(cacheFile);
    }

    void CacheManager::InvalidateFromCacheTable(const std::string_view cacheFile)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        // Remove from table
        {
            const std::scoped_lock lock{m_mutex};

            m_cacheTable.erase(cacheFile);
        }

        if (!Util::Files::Remove(cacheFile))
        {
            Logger::Warning("Failed to remove cached file! [File={}]\n", cacheFile);
        }
    }

    bool CacheManager::IsInCacheTable(const std::string_view cacheFile)
    {
        const std::scoped_lock lock{m_mutex};

        return m_cacheTable.contains(cacheFile);
    }

    bool CacheHeader::Validate() const
    {
        return magic == CACHE_HEADER_MAGIC &&
               version == CACHE_HEADER_VERSION &&
               headerSize != 0 &&
               compressedDataSize != 0 &&
               uncompressedDataSize != 0;
    }

    bool CachedTextureHeader::Validate() const
    {
        return width != 0 &&
               height != 0 &&
               format != VK_FORMAT_UNDEFINED;
    }
}