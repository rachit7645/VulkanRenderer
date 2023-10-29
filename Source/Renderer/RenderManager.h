#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include <memory>
#include <vulkan/vulkan.h>

#include "Vulkan/Context.h"
#include "Engine/Window.h"
#include "Util/Util.h"

namespace Renderer
{
    class RenderManager
    {
    public:
        // Constructor
        explicit RenderManager(const std::shared_ptr<Engine::Window>& window);

        // Render frame
        void Render();

        // Begin frame
        void BeginFrame();
        // End frame
        void EndFrame();
        // Begin render pass
        void BeginRenderPass();
        // End render pass
        void EndRenderPass();

        // Present
        void Present();

        void WaitForLogicalDevice();

    private:
        // Wait for previous frame
        void WaitForFrame();
        // Acquire swap chain image
        void AcquireSwapChainImage();
        // Submit queue
        void SubmitQueue();

        // Vulkan context
        std::unique_ptr<Vk::Context> m_vkContext = nullptr;
        // Image index
        u32 m_imageIndex = 0;
    };
}

#endif