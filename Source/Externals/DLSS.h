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

#ifndef EXTERNALS_DLSS_H
#define EXTERNALS_DLSS_H

#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wsign-compare"
    #pragma clang diagnostic ignored "-Wmissing-field-initializers"
    #pragma clang diagnostic ignored "-Wmissing-braces"
#endif

#include "dlss/include/nvsdk_ngx.h"
#include "dlss/include/nvsdk_ngx_defs.h"
#include "dlss/include/nvsdk_ngx_params.h"
#include "dlss/include/nvsdk_ngx_helpers.h"
#include "dlss/include/nvsdk_ngx_vk.h"
#include "dlss/include/nvsdk_ngx_helpers_vk.h"

#include "Vulkan/Constants.h"

namespace DLSS
{
    void NVSDK_CONV DebugCallback(const char* message, NVSDK_NGX_Logging_Level level, NVSDK_NGX_Feature feature);

    constexpr auto PROJECT_ID            = "b33fcf5d-5bba-4094-ad26-2060f00bfdae";
    constexpr auto ENGINE_VERSION        = "0.0.0.1";
    constexpr auto APPLICATION_DATA_PATH = L".";

    constexpr NVSDK_NGX_FeatureCommonInfo FEATURE_COMMON_INFO =
    {
        .PathListInfo = {
            .Path   = nullptr,
            .Length = 0
        },
        .InternalData = {},
        .LoggingInfo  = {
            .LoggingCallback          = &DLSS::DebugCallback,
            .MinimumLoggingLevel      = NVSDK_NGX_LOGGING_LEVEL_ON,
            .DisableOtherLoggingSinks = true
        }
    };

    constexpr NVSDK_NGX_FeatureDiscoveryInfo FEATURE_DISCOVERY_INFO =
    {
        .SDKVersion          = NVSDK_NGX_Version_API,
        .FeatureID           = NVSDK_NGX_Feature_SuperSampling,
        .Identifier          = {
            .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id,
            .v              = {.ProjectDesc = {
                .ProjectId     = DLSS::PROJECT_ID,
                .EngineType    = NVSDK_NGX_ENGINE_TYPE_CUSTOM,
                .EngineVersion = ENGINE_VERSION
            }}
        },
        .ApplicationDataPath = DLSS::APPLICATION_DATA_PATH,
        .FeatureInfo         = &DLSS::FEATURE_COMMON_INFO
    };
}

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

#endif