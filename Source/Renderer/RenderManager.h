#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include <memory>
#include <vulkan/vulkan.h>

#include "RenderPipeline.h"
#include "Vulkan/Context.h"
#include "Engine/Window.h"
#include "Util/Util.h"

namespace Renderer
{
    class RenderManager
    {
    public:
        // Constructor
        explicit RenderManager(std::shared_ptr<Engine::Window> window);

        // Render frame
        void Render();

        // Begin frame
        void BeginFrame();
        // End frame
        void EndFrame();

        // Present
        void Present();
        // Wait for gpu to finish
        void WaitForLogicalDevice();
    private:
        // Wait for previous frame
        void WaitForFrame();
        // Acquire swap chain image
        void AcquireSwapChainImage();
        // Submit queue
        void SubmitQueue();

        // Pointer to window
        std::shared_ptr<Engine::Window> m_window = nullptr;
        // Vulkan context
        std::shared_ptr<Vk::Context> m_vkContext = nullptr;
        // Render pipeline
        std::shared_ptr<Renderer::RenderPipeline> m_renderPipeline = nullptr;

        // Image index
        u32 m_imageIndex = 0;
        // Frame index
        usize currentFrame = 0;
    };
}

#endif