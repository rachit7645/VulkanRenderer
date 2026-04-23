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
#include "Vulkan/Texture.h"

namespace Engine
{
    constexpr auto CACHE_DIRECTORY = "Cache/";

    constexpr u32 CACHE_HEADER_MAGIC   = 0x4B4F4F43;
    constexpr u8  CACHE_HEADER_VERSION = 1;

    u64 CachedAssetTypeToHeaderSize(Engine::CachedAssetType assetType)
    {
        switch (assetType)
        {
        case CachedAssetType::Texture:
            return sizeof(Engine::CachedTextureHeader);

        default:
            Logger::Error("{}\n", "Unknown cached asset type!");
        }
    }

    u64 CachedTextureSourceToHash(Engine::CachedTextureSource source)
    {
        switch (source)
        {
        case CachedTextureSource::File:
            Logger::Error("{}\n", "Not implemented!");

        case CachedTextureSource::BRDF_LUT:
        {
            u64 hash = 0;

            hash = Util::HashCombine(hash, Renderer::IBL::BRDF_LUT_SIZE.x);
            hash = Util::HashCombine(hash, Renderer::IBL::BRDF_LUT_SIZE.y);
            hash = Util::HashCombine(hash, Renderer::IBL::BRDF_LUT_FORMAT);
            hash = Util::HashCombine(hash, Renderer::IBL::BRDF::BRDF_LUT_SAMPLE_COUNT);

            return hash;
        }

        default:
            Logger::Error("{}\n", "Unknown cached asset type!");
        }
    }

    CacheManager::CacheManager()
    {
        for (const auto& file : Util::Files::GetFilesInDirectory(CACHE_DIRECTORY))
        {
            m_cacheTable.emplace(file);
        }
    }

    void CacheManager::InsertIntoCache(const CacheEntry& entry)
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

        Engine::CacheHeader header =
        {
            .magic         = CACHE_HEADER_MAGIC,
            .version       = CACHE_HEADER_VERSION,
            .assetType     = entry.assetType,
            .pad0          = 0,
            .headerSize    = CachedAssetTypeToHeaderSize(entry.assetType),
            .dataSize      = entry.data.size(),
            .hash          = 0
        };

        std::visit(Util::Visitor{
            [&header] (const Engine::CachedTextureHeader& textureHeader)
            {
                header.hash = CachedTextureSourceToHash(textureHeader.source);
            }
        }, entry.assetHeader);

        bin.write(reinterpret_cast<const char*>(&header), sizeof(Engine::CacheHeader));

        std::visit(Util::Visitor{
            [&bin] (const Engine::CachedTextureHeader& textureHeader)
            {
                bin.write(reinterpret_cast<const char*>(&textureHeader), sizeof(Engine::CachedTextureHeader));
            }
        }, entry.assetHeader);

        bin.write(reinterpret_cast<const char*>(entry.data.data()), static_cast<std::streamsize>(entry.data.size()));

        bin.flush();
        bin.close();

        AppendToCacheTable(path);
    }

    bool CacheManager::IsInCache(const std::string_view file, Engine::CachedAssetType assetType)
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", file.data());
        #endif

        const std::string path = CACHE_DIRECTORY + std::string(file);

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

        if (header.assetType != assetType)
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

            if (header.hash != CachedTextureSourceToHash(textureHeader.source))
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

        auto data = std::vector<u8>(header.dataSize);

        bin.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

        return Engine::CacheEntry
        {
            .sourceFile  = std::nullopt,
            .cacheFile   = file.data(),
            .assetType   = header.assetType,
            .assetHeader = assetHeader,
            .data        = std::move(data)
        };
    }

    void CacheManager::AppendToCacheTable(const std::string_view cacheFile)
    {
        const std::scoped_lock lock{m_mutex};

        m_cacheTable.emplace(cacheFile);
    }

    // TODO: Deferred Invalidation (To avoid filesystem operations on the main thread)
    void CacheManager::InvalidateFromCacheTable(const std::string_view cacheFile)
    {
        const std::scoped_lock lock{m_mutex};

        m_cacheTable.erase(cacheFile);

        if (!std::filesystem::remove(cacheFile))
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
               assetType == Engine::CachedAssetType::Texture &&
               headerSize != 0 &&
               dataSize != 0;
    }

    bool CachedTextureHeader::Validate() const
    {
        return width != 0 &&
               height != 0 &&
               format != VK_FORMAT_UNDEFINED &&
               (source == CachedTextureSource::File || source == CachedTextureSource::BRDF_LUT);
    }
}