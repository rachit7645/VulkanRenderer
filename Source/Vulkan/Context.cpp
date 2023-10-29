#include "Context.h"

#include <map>
#include <unordered_map>
#include <limits>

#include "Extensions.h"
#include "Util/Log.h"
#include "Externals/GLM.h"

namespace Vk
{
    // TODO: Figure out a good way to move both of these vectors to their respective classes

    // Required device extensions
    const std::vector<const char*> REQUIRED_EXTENSIONS = {"VK_KHR_swapchain"};
    #ifdef ENGINE_DEBUG
    // Layers
    const std::vector<const char*> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
    #endif

    Context::Context(SDL_Window* window)
        : m_extensions(std::make_unique<Vk::Extensions>())
    {
        // Create vulkan instance
        CreateVKInstance(window);
        // Load extensions
        m_extensions->LoadFunctions(vkInstance);

        #ifdef ENGINE_DEBUG
        // Load validation layers
        if (m_layers->SetupMessenger(vkInstance) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("{}\n", "Failed to set up debug messenger!");
        }
        #endif

        // Create surface
        CreateSurface(window);
        // Grab a GPU
        PickPhysicalDevice();
        // Setup logical device
        CreateLogicalDevice();

        // Create swap chain
        CreateSwapChain(window);
        // Create image views for swap chain
        CreateImageViews();

        // Create render pass
        CreateRenderPass();
        // Create framebuffers
        CreateFramebuffers();

        // Create global command pool
        CreateCommandPool();
        // Creates command buffer
        CreateCommandBuffer();
        // Create sync objects
        CreateSyncObjects();

        // Log
        LOG_INFO("{}\n", "Initialised vulkan context!");
    }

    void Context::CreateVKInstance(SDL_Window* window)
    {
        // Application info
        VkApplicationInfo appInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "Vulkan Renderer",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "Rachit's Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_0
        };

        // Get extensions
        auto extensions = m_extensions->LoadInstanceExtensions(window);
        // Create extensions string
        std::string extDbg = fmt::format("Loaded vulkan instance extensions: {}\n", extensions.size());
        for (auto&& extension : extensions)
        {
            // Add
            extDbg.append(fmt::format("- {}\n", extension));
        }
        // Log
        LOG_DEBUG("{}", extDbg);

        #ifdef ENGINE_DEBUG
        // Request validation layers
        m_layers = std::make_unique<Vk::ValidationLayers>(VALIDATION_LAYERS);
        #endif

        // Instance creation info
        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            #ifdef ENGINE_DEBUG
            .pNext                   = (VkDebugUtilsMessengerCreateInfoEXT*) &m_layers->messengerInfo,
            #else
            .pNext                   = nullptr,
            #endif
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            #ifdef ENGINE_DEBUG
            .enabledLayerCount       = static_cast<u32>(VALIDATION_LAYERS.size()),
            .ppEnabledLayerNames     = VALIDATION_LAYERS.data(),
            #else
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            #endif
            .enabledExtensionCount   = static_cast<u32>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        // Create instance
        if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS)
        {
            // Vulkan initialisation failed, abort
            LOG_ERROR("{}\n", "Failed to initialise vulkan instance!");
        }

        // Log
        LOG_INFO("Successfully initialised Vulkan instance! [handle={}]\n", reinterpret_cast<void*>(vkInstance));
    }

    void Context::CreateSurface(SDL_Window* window)
    {
        // Create surface using SDL
        if (SDL_Vulkan_CreateSurface(window, vkInstance, &m_surface) != SDL_TRUE)
        {
            // Log
            LOG_ERROR
            (
                "Failed to create surface! [window={}] [instance={}]\n",
                reinterpret_cast<void*>(window),
                reinterpret_cast<void*>(vkInstance)
            );
        }

        // Log
        LOG_INFO("Initialised vulkan surface! [handle={}]\n", reinterpret_cast<void*>(m_surface));
    }

    void Context::PickPhysicalDevice()
    {
        // Get device count
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

        // Make sure we have at least one GPU that supports Vulkan
        if (deviceCount == 0)
        {
            // Log
            LOG_ERROR("No physical devices found! [instance = {}]\n", reinterpret_cast<void*>(vkInstance));
        }

        // Physical devices
        auto devices = std::vector<VkPhysicalDevice>(deviceCount);
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

        // Debug list
        std::string dbgPhyDevices = fmt::format("Available physical devices: {}\n", deviceCount);
        // Devices data
        auto properties = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceProperties>(deviceCount);
        auto features   = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceFeatures>(deviceCount);
        // Device scores
        auto scores = std::map<usize, VkPhysicalDevice>{};


        // Get information about physical devices
        for (auto&& currentDevice : devices)
        {
            // Get data
            VkPhysicalDeviceProperties propertySet;
            vkGetPhysicalDeviceProperties(currentDevice, &propertySet);
            VkPhysicalDeviceFeatures featureSet;
            vkGetPhysicalDeviceFeatures(currentDevice, &featureSet);

            // Load data
            properties.emplace(currentDevice, propertySet);
            features.emplace(currentDevice, featureSet);

            // Add to string
            dbgPhyDevices.append(fmt::format
            (
                "- {} [Driver Version: {}] [API Version: {}] [ID = {}] \n",
                propertySet.deviceName,
                propertySet.driverVersion,
                propertySet.apiVersion,
                propertySet.deviceID
            ));

            // Add score
            scores.emplace(CalculateScore(currentDevice, propertySet), currentDevice);
        }

        // Log string
        LOG_DEBUG("{}", dbgPhyDevices);

        // Best GPU => Highest score
        auto [highestScore, bestDevice] = *scores.rbegin();
        // Make sure device is valid
        if (highestScore == 0)
        {
            // Log
            LOG_ERROR("Failed to load physical device! [name=\"{}\"]", properties[bestDevice].deviceName);
        }
        // Set GPU
        m_physicalDevice = bestDevice;

        // Log
        LOG_INFO("Selecting GPU: {}\n", properties[m_physicalDevice].deviceName);
    }

    usize Context::CalculateScore(VkPhysicalDevice logicalDevice, VkPhysicalDeviceProperties propertySet)
    {
        // Get queue
        auto queue = QueueFamilyIndices(logicalDevice, m_surface);

        // Calculate score parts
        usize discreteGPU = (propertySet.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 10000;

        // Calculate score multipliers
        bool isQueueValid  = queue.IsComplete();
        bool hasExtensions = m_extensions->CheckDeviceExtensionSupport(logicalDevice, REQUIRED_EXTENSIONS);

        // Need extensions to calculate these
        bool isSwapChainAdequate = true;

        // Calculate
        if (hasExtensions)
        {
            // Make sure we have valid presentation and surface formats
            auto swapChainInfo = SwapChainInfo(logicalDevice, m_surface);
            isSwapChainAdequate = !(swapChainInfo.formats.empty() || swapChainInfo.presentModes.empty());
        }

        // Calculate score
        return hasExtensions * isQueueValid * isSwapChainAdequate * discreteGPU;
    }

    void Context::CreateLogicalDevice()
    {
        // Get queue
        m_queueFamilies = QueueFamilyIndices(m_physicalDevice, m_surface);

        // Queue priority
        constexpr f32 queuePriority = 1.0f;

        // Queues data
        auto uniqueQueueFamilies = m_queueFamilies.GetUniqueFamilies();
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        // Create queue creation data
        for (auto queueFamily : uniqueQueueFamilies)
        {
            // Queue creation info
            VkDeviceQueueCreateInfo queueCreateInfo =
            {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .queueFamilyIndex = queueFamily,
                .queueCount       = 1,
                .pQueuePriorities = &queuePriority
            };
            // Add to vector
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Vulkan device features
        VkPhysicalDeviceFeatures deviceFeatures = {};

        // Logical device creation info
        VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = static_cast<u32>(queueCreateInfos.size()),
            .pQueueCreateInfos       = queueCreateInfos.data(),
        #ifdef ENGINE_DEBUG
            .enabledLayerCount       = static_cast<u32>(VALIDATION_LAYERS.size()),
            .ppEnabledLayerNames     = VALIDATION_LAYERS.data(),
        #else
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
        #endif
            .enabledExtensionCount   = static_cast<u32>(REQUIRED_EXTENSIONS.size()),
            .ppEnabledExtensionNames = REQUIRED_EXTENSIONS.data(),
            .pEnabledFeatures        = &deviceFeatures
        };

        // Create device
        if (vkCreateDevice(
            m_physicalDevice,
            &createInfo,
            nullptr,
            &device
        ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create logical device! [Physical Device = {}]\n", reinterpret_cast<void*>(m_physicalDevice));
        }

        // Log
        LOG_INFO("Successfully created vulkan logical device! [handle={}]\n", reinterpret_cast<void*>(device));

        // Get queue
        vkGetDeviceQueue
        (
            device,
            m_queueFamilies.graphicsFamily.value(),
            0,
            &graphicsQueue
        );
    }

    void Context::CreateSwapChain(SDL_Window* window)
    {
        // Get swap chain info
        auto swapChainInfo = SwapChainInfo(m_physicalDevice, m_surface);
        // Get swap chain config data
        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(swapChainInfo);
        VkPresentModeKHR   presentMode   = ChoosePresentationMode(swapChainInfo);
        VkExtent2D         extent        = ChooseSwapExtent(window, swapChainInfo);

        // Get image count
        u32 imageCount = glm::max
        (
            swapChainInfo.capabilities.minImageCount + 1,
            swapChainInfo.capabilities.maxImageCount
        );

        // Swap chain creation data
        VkSwapchainCreateInfoKHR createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = m_surface,
            .minImageCount         = imageCount,
            .imageFormat           = surfaceFormat.format,
            .imageColorSpace       = surfaceFormat.colorSpace,
            .imageExtent           = extent,
            .imageArrayLayers      = 1,
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = swapChainInfo.capabilities.currentTransform,
            .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode           = presentMode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = VK_NULL_HANDLE
        };

        // Create swap chain
        if (vkCreateSwapchainKHR(
                device,
                &createInfo,
                nullptr,
                &swapChain
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create swap chain! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Get image count
        vkGetSwapchainImagesKHR
        (
            device,
            swapChain,
            &imageCount,
            nullptr
        );
        // Resize
        m_swapChainImages.resize(imageCount);
        // Get the images
        vkGetSwapchainImagesKHR
        (
            device,
            swapChain,
            &imageCount,
            m_swapChainImages.data()
        );

        // Store other properties
        m_swapChainImageFormat = surfaceFormat.format;
        swapChainExtent        = extent;

        // Log
        LOG_INFO("Initialised swap chain! [handle={}]\n", reinterpret_cast<void*>(swapChain));
    }

    void Context::CreateImageViews()
    {
        // Resize
        m_swapChainImageViews.resize(m_swapChainImages.size());

        // Loop over all swap chain images
        for (usize i = 0; i < m_swapChainImages.size(); ++i)
        {
            // Image view creation data
            VkImageViewCreateInfo createInfo =
            {
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .image            = m_swapChainImages[i],
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = m_swapChainImageFormat,
                .components       = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            };
            // Create image view
            if (vkCreateImageView(
                    device,
                    &createInfo,
                    nullptr,
                    &m_swapChainImageViews[i]
                ) != VK_SUCCESS)
            {
                // Log
                LOG_ERROR
                (
                    "Failed to create image view #{}! [device={}] [image={}]",
                    i, reinterpret_cast<void*>(device),
                    reinterpret_cast<void*>(m_swapChainImages[i])
                );
            }
        }
    }

    VkSurfaceFormatKHR Context::ChooseSurfaceFormat(const SwapChainInfo& swapChainInfo)
    {
        // Formats
        const auto& formats = swapChainInfo.formats;

        // Search
        for (auto&& availableFormat : formats)
        {
            // Check
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && // BGRA is faster or something IDK
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) // SRGB Buffer
            {
                // Found preferred format!
                return availableFormat;
            }
        }

        // By default, return the first format
        return formats[0];
    }

    VkPresentModeKHR Context::ChoosePresentationMode(const SwapChainInfo& swapChainInfo)
    {
        // Presentation modes
        const auto& presentModes = swapChainInfo.presentModes;

        // Check all presentation modes
        for (auto&& presentMode : presentModes)
        {
            // Check for mailbox
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                // Found it!
                return presentMode;
            }
        }

        // FIFO is guaranteed to be supported
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Context::ChooseSwapExtent(SDL_Window* window, const SwapChainInfo& swapChainInfo)
    {
        // Surface capability data
        const auto& capabilities = swapChainInfo.capabilities;

        // Some platforms set swap extents themselves
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max())
        {
            // Just give back the original swap extent
            return capabilities.currentExtent;
        }

        // Window pixel size
        glm::ivec2 size = {};
        SDL_Vulkan_GetDrawableSize(window, &size.x, &size.y);

        // Get min and max
        auto minSize = glm::ivec2(capabilities.minImageExtent.width, capabilities.minImageExtent.height);
        auto maxSize = glm::ivec2(capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);

        // Clamp
        auto actualExtent = glm::clamp(size, minSize, maxSize);

        // Return
        return
        {
            .width  = static_cast<u32>(actualExtent.x),
            .height = static_cast<u32>(actualExtent.y),
        };
    }

    void Context::CreateRenderPass()
    {
        // Color attachment
        VkAttachmentDescription colorAttachment =
        {
            .flags          = 0,
            .format         = m_swapChainImageFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        // Color attachment ref
        VkAttachmentReference colorAttachmentRef =
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        // Subpass description
        VkSubpassDescription subpass =
        {
            .flags                   = 0,
            .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount    = 0,
            .pInputAttachments       = nullptr,
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &colorAttachmentRef,
            .pResolveAttachments     = nullptr,
            .pDepthStencilAttachment = nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments    = nullptr
        };

        // Subpass dependency info
        VkSubpassDependency dependency =
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = 0,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        };

        // Render pass creation info
        VkRenderPassCreateInfo createInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .attachmentCount = 1,
            .pAttachments    = &colorAttachment,
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = 1,
            .pDependencies   = &dependency
        };

        // Create render pass
        if (vkCreateRenderPass(
                device,
                &createInfo,
                nullptr,
                &renderPass
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create render pass! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        LOG_INFO("Created render pass! [handle={}]\n", reinterpret_cast<void*>(renderPass));
    }

    void Context::CreateFramebuffers()
    {
        // Resize
        swapChainFrameBuffers.resize(m_swapChainImageViews.size());

        // For each image view
        for (usize i = 0; i < m_swapChainImageViews.size(); ++i)
        {
            // Framebuffer info
            VkFramebufferCreateInfo framebufferInfo =
            {
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext           = nullptr,
                .flags           = 0,
                .renderPass      = renderPass,
                .attachmentCount = 1,
                .pAttachments    = &m_swapChainImageViews[i],
                .width           = swapChainExtent.width,
                .height          = swapChainExtent.height,
                .layers          = 1
            };

            // Create framebuffer
            if (vkCreateFramebuffer(
                    device,
                    &framebufferInfo,
                    nullptr,
                    &swapChainFrameBuffers[i]
                ) != VK_SUCCESS)
            {
                // Log
                LOG_ERROR
                (
                    "Failed to create framebuffer #{}! [image={}]\n",
                    i, reinterpret_cast<void*>(&m_swapChainImageViews[i])
                );
            }
        }

        // Log
        LOG_INFO("{}\n", "Created framebuffers!");
    }

    void Context::CreateCommandPool()
    {
        // Command pool info
        VkCommandPoolCreateInfo createInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_queueFamilies.graphicsFamily.value()
        };

        // Create command pool
        if (vkCreateCommandPool(
                device,
                &createInfo,
                nullptr,
                &m_commandPool
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create command pool! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        LOG_INFO("Created command pool! [handle={}]\n", reinterpret_cast<void*>(m_commandPool));
    }

    void Context::CreateCommandBuffer()
    {
        // Allocation info
        VkCommandBufferAllocateInfo allocInfo
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = m_commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        // Allocate command buffer
        if (vkAllocateCommandBuffers(
                device,
                &allocInfo,
                &commandBuffer
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to allocate command buffer(s)! [CommandPool={}]\n",
                reinterpret_cast<void*>(m_commandPool)
            );
        }

        // Log
        LOG_INFO("Created command buffer! [handle={}]\n", reinterpret_cast<void*>(commandBuffer));
    }

    void Context::CreateSyncObjects()
    {
        // Semaphore info
        VkSemaphoreCreateInfo semaphoreInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        // Fence info
        VkFenceCreateInfo fenceInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        // Create sync objects
        if
        (
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlight) != VK_SUCCESS
        )
        {
            // Log
            LOG_ERROR("{}\n", "Failed to create sync objects!");
        }

        // Log
        LOG_INFO("{}\n", "Created synchronisation objects!");
    }

    Context::~Context()
    {
        // Destroy semaphores
        vkDestroySemaphore(device, imageAvailable, nullptr);
        vkDestroySemaphore(device, renderFinished, nullptr);
        // Destroy fences
        vkDestroyFence(device, inFlight, nullptr);

        // Destroy command pool
        vkDestroyCommandPool(device, m_commandPool, nullptr);

        // Destroy framebuffers
        for (auto&& framebuffer : swapChainFrameBuffers)
        {
            // Destroy
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        // Destroy render pass
        vkDestroyRenderPass(device, renderPass, nullptr);

        // Destroy swap chain images
        for (auto&& imageView : m_swapChainImageViews)
        {
            // Delete
            vkDestroyImageView(device, imageView, nullptr);
        }

        // Destroy swap chain
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        // Destroy surface
        vkDestroySurfaceKHR(vkInstance, m_surface, nullptr);

        #ifdef ENGINE_DEBUG
        // Destroy validation layers
        m_layers->DestroyMessenger(vkInstance);
        #endif

        // Destroy logical device
        vkDestroyDevice(device, nullptr);
        // Destroy vulkan instance
        vkDestroyInstance(vkInstance, nullptr);

        // Log
        LOG_INFO("{}\n", "Destroyed vulkan context!");
    }
}