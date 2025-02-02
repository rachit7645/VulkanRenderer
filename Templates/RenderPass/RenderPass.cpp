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

#include "RenderPass.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::RenderPass
{
    Pass::Pass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet,
        VkExtent2D extent
    )
        : pipeline(context, formatHelper, megaSet)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("DepthPass/FIF{}", i));
        }

        InitData(context, formatHelper, extent);

        Logger::Info("{}\n", "Created depth pass!");
    }

    void Pass::Recreate
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        VkExtent2D extent
    )
    {
        m_deletionQueue.FlushQueue();

        InitData(context, formatHelper, extent);

        Logger::Info("{}\n", "Recreated depth pass!");
    }

    void Pass::InitData
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        VkExtent2D extent
    )
    {
        m_renderSize = {extent.width, extent.height};

        m_deletionQueue.PushDeletor([&] ()
        {
        });
    }

    void Pass::Render()
    {
    }

    void Pass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying depth pass!");

        m_deletionQueue.FlushQueue();

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        pipeline.Destroy(device);
    }
}
