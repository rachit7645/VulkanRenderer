#ifndef RENDER_PIPELINE_H
#define RENDER_PIPELINE_H

#include <memory>
#include <vulkan/vulkan.h>

#include "Vulkan/Context.h"

namespace Renderer
{
    class RenderPipeline
    {
    public:
        // Create render pipeline
        RenderPipeline(const std::shared_ptr<Vk::Context>& vkContext);
        // Destroy render pipeline
        ~RenderPipeline();

        // Pipeline data
        VkPipeline pipeline = {};
        // Layout
        VkPipelineLayout pipelineLayout = {};
    private:
        VkDevice m_device = {};
    };
}

#endif
