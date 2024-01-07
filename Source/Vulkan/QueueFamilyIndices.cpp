/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "QueueFamilyIndices.h"
#include "Util/Log.h"
#include "Util.h"

namespace Vk
{
    QueueFamilyIndices::QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2
        (
            device,
            &queueFamilyCount,
            nullptr
        );

        if (queueFamilyCount == 0)
        {
            Logger::VulkanError("Failed to find any queue families! [device={}]\n", std::bit_cast<void*>(device));
        }

        VkQueueFamilyProperties2 emptyQueue = {};
        emptyQueue.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        emptyQueue.pNext = nullptr;

        auto queueFamilies = std::vector<VkQueueFamilyProperties2>(queueFamilyCount, emptyQueue);
        vkGetPhysicalDeviceQueueFamilyProperties2
        (
            device,
            &queueFamilyCount,
            queueFamilies.data()
        );

        for (u32 i = 0; i < queueFamilies.size(); ++i)
        {
            VkBool32 presentSupport = VK_FALSE;
            Vk::CheckResult(vkGetPhysicalDeviceSurfaceSupportKHR(
                device,
                i,
                 surface,
                &presentSupport),
                "Failed to check for surface support!"
            );

            if (presentSupport == VK_TRUE && queueFamilies[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                // Found graphics family!
                graphicsFamily = i;
            }

            if (IsComplete())
            {
                // Found all the queue families we need
                break;
            }
        }
    }

    std::set<u32> QueueFamilyIndices::GetUniqueFamilies() const
    {
        return {graphicsFamily.value_or(0)};
    }

    bool QueueFamilyIndices::IsComplete() const
    {
        return graphicsFamily.has_value();
    }
}