/*
 * Copyright (c) 2023 - 2025 Rachit
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

#ifndef EXTENSION_STATE_H
#define EXTENSION_STATE_H

#include <vulkan/vulkan.h>

namespace Vk
{
    struct ExtensionState
    {
        PFN_vkCreateDebugUtilsMessengerEXT  p_CreateDebugUtilsMessengerEXT  = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT p_DestroyDebugUtilsMessengerEXT = nullptr;
        PFN_vkCmdBeginDebugUtilsLabelEXT    p_CmdBeginDebugUtilsLabelEXT    = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT      p_CmdEndDebugUtilsLabelEXT      = nullptr;
        PFN_vkQueueBeginDebugUtilsLabelEXT  p_QueueBeginDebugUtilsLabelEXT  = nullptr;
        PFN_vkQueueEndDebugUtilsLabelEXT    p_QueueEndDebugUtilsLabelEXT    = nullptr;
        PFN_vkSetDebugUtilsObjectNameEXT    p_SetDebugUtilsObjectNameEXT    = nullptr;
    };
}

#endif
