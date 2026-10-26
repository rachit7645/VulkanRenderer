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

#include "Properties.h"

namespace Vk
{
    Properties::Properties
    (
        const VkPhysicalDeviceLimits& limits,
        const VkPhysicalDeviceVulkan12Properties& vk12Properties,
        const VkPhysicalDeviceAccelerationStructurePropertiesKHR& asProperties,
        const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProperties
    )
        : maxSamplerAnisotropy{limits.maxSamplerAnisotropy},
          maxPerStageDescriptorUpdateAfterBindSamplers{vk12Properties.maxPerStageDescriptorUpdateAfterBindSamplers},
          maxPerStageDescriptorUpdateAfterBindSampledImages{vk12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages},
          maxPerStageDescriptorUpdateAfterBindStorageImages{vk12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages},
          minAccelerationStructureScratchOffsetAlignment{asProperties.minAccelerationStructureScratchOffsetAlignment},
          shaderGroupHandleSize{rtProperties.shaderGroupHandleSize},
          shaderGroupBaseAlignment{rtProperties.shaderGroupBaseAlignment},
          shaderGroupHandleAlignment{rtProperties.shaderGroupHandleAlignment}
    {
    }
}