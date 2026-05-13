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

#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include "Externals/UnorderedDense.h"
#include "Util/String.h"
#include "Util/Types.h"

namespace Engine
{
    enum class CachedAssetType : u8
    {
        Texture
    };

    struct CacheHeader
    {
        [[nodiscard]] bool Validate() const;

        u32             magic                = 0;
        u8              version              = 0;
        CachedAssetType assetType            = CachedAssetType::Texture;
        u16             pad0                 = 0;
        u64             headerSize           = 0;
        u64             compressedDataSize   = 0;
        u64             uncompressedDataSize = 0;
        u64             hash                 = 0;
    };

    struct CachedTextureHeader
    {
        [[nodiscard]] bool Validate() const;

        u32      width   = 0;
        u32      height  = 0;
        VkFormat format  = VK_FORMAT_UNDEFINED;
    };

    using CachedAssetHeader = std::variant<CachedTextureHeader>;

    struct CacheEntry
    {
        std::string             cacheFile   = "Null/Cache";
        Engine::CachedAssetType assetType   = CachedAssetType::Texture;
        CachedAssetHeader       assetHeader = {};
        u64                     hash        = 0;
        std::vector<u8>         data        = {};
    };

    struct CacheLookup
    {
        std::string             cachedFile = "Null/Cache";
        Engine::CachedAssetType assetType  = CachedAssetType::Texture;
        u64                     hash        = 0;
    };

    class CacheManager
    {
    public:
        CacheManager();

        void InsertIntoCache(const Engine::CacheEntry& entry);

        // Checks and validates cached file
        [[nodiscard]] bool IsInCache(const Engine::CacheLookup& lookup);

        // UNSAFE: Loads data from cached file WITHOUT DOING ANY VALIDATION, call IsInCache before calling this!
        [[nodiscard]] Engine::CacheEntry GetFromCache(const std::string_view file) const;
    private:
        void AppendToCacheTable(const std::string_view cacheFile);
        void InvalidateFromCacheTable(const std::string_view cacheFile);

        [[nodiscard]] bool IsInCacheTable(const std::string_view cacheFile);

        ankerl::unordered_dense::set<std::string, Util::StringHash, std::equal_to<>> m_cacheTable;

        std::mutex m_mutex;
    };
}

#endif