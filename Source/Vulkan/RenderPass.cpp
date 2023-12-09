/*
 *    Copyright 2023 Rachit Khandelwal
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

#include "RenderPass.h"

namespace Vk
{
    RenderPass::RenderPass(VkRenderPass renderPass)
        : handle(renderPass)
    {
    }

    void RenderPass::ResetClearValues()
    {
        // Reset
        clearValues.clear();
    }

    void RenderPass::SetClearValue(const glm::vec4& color)
    {
        // Put into clear value
        VkClearColorValue clearColor = {.float32 = {color.r, color.g, color.b, color.a}};
        // Set
        clearValues.emplace_back(clearColor);
    }

    void RenderPass::SetClearValue(f32 depth, u32 stencil)
    {
        // Put into depth stencil value
        VkClearValue depthStencil = {.depthStencil = {.depth = depth, .stencil = stencil}};
        // Set
        clearValues.emplace_back(depthStencil);
    }

    void RenderPass::BeginRenderPass
    (
        VkCommandBuffer cmdBuffer,
        const Framebuffer& framebuffer,
        VkRect2D renderArea,
        VkSubpassContents subpassContents
    )
    {
        // Render pass begin info
        VkRenderPassBeginInfo renderPassInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext           = nullptr,
            .renderPass      = handle,
            .framebuffer     = framebuffer.handle,
            .renderArea      = renderArea,
            .clearValueCount = static_cast<u32>(clearValues.size()),
            .pClearValues    = clearValues.data()
        };

        // Begin render pass
        vkCmdBeginRenderPass
        (
            cmdBuffer,
            &renderPassInfo,
            subpassContents
        );
    }

    void RenderPass::Destroy(VkDevice device) const
    {
        // Destroy
        vkDestroyRenderPass(device, handle, nullptr);
    }
}