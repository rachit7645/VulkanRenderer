#include "RenderPipeline.h"

#include "Vulkan/PipelineBuilder.h"

namespace Renderer
{
    RenderPipeline::RenderPipeline(const std::shared_ptr<Vk::Context>& vkContext)
        : m_device(vkContext->device)
    {
        // Custom functions
        auto SetDynamicStates = [vkContext](Vk::PipelineBuilder& pipelineBuilder)
        {
            // Set viewport config
            pipelineBuilder.viewport =
            {
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<f32>(vkContext->swapChainExtent.width),
                .height   = static_cast<f32>(vkContext->swapChainExtent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            // Set scissor config
            pipelineBuilder.scissor =
            {
                .offset = {0, 0},
                .extent = vkContext->swapChainExtent
            };

            // Create viewport creation info
            pipelineBuilder.viewportInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .pNext         = nullptr,
                .flags         = 0,
                .viewportCount = 1,
                .pViewports    = &pipelineBuilder.viewport,
                .scissorCount  = 1,
                .pScissors     = &pipelineBuilder.scissor
            };
        };

        // Dynamic states
        auto dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        // Pipeline data tuple
        std::tuple<VkPipeline, VkPipelineLayout> pipelineData = {};

        // Build pipeline
        pipelineData = Vk::PipelineBuilder::Create(vkContext->device, vkContext->renderPass)
                       .AttachShader("BasicShader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
                       .AttachShader("BasicShader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
                       .SetDynamicStates(dynStates, SetDynamicStates)
                       .SetVertexInputState()
                       .SetIAState()
                       .SetRasterizerState(VK_CULL_MODE_BACK_BIT)
                       .SetMSAAState()
                       .SetBlendState()
                       .Build();

        // Retrieve members
        std::tie(pipeline, pipelineLayout) = pipelineData;
    }

    RenderPipeline::~RenderPipeline()
    {
        // Destroy pipeline
        vkDestroyPipeline(m_device, pipeline, nullptr);
        // Destroy pipeline layout
        vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
    }
}