cmake_minimum_required(VERSION 3.25)
project(VulkanRenderer)

# Install location
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(INSTALL_DIR ${CMAKE_SOURCE_DIR}/Bin/)

# C++ 20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(SDL2     REQUIRED)
find_package(Vulkan   REQUIRED)
find_package(glm      REQUIRED)
find_package(assimp   REQUIRED)
find_package(fmt      REQUIRED)

# Add sub-projects
add_subdirectory(Externals/)
add_subdirectory(Assets/Shaders/)

# Main executable
add_executable(VulkanRenderer
    # Main sources
	Source/Main.cpp
	# Engine sources
	Source/Engine/AppInstance.cpp
	Source/Engine/Window.cpp
	Source/Engine/Inputs.cpp
    # Utility Sources
	Source/Util/Files.cpp
	Source/Util/Time.cpp
	Source/Util/DeletionQueue.cpp
	Source/Util/FrameCounter.cpp
	Source/Util/Maths.cpp
	# Vulkan sources
	Source/Vulkan/Extensions.cpp
	Source/Vulkan/ValidationLayers.cpp
	Source/Vulkan/Context.cpp
	Source/Vulkan/QueueFamilyIndices.cpp
	Source/Vulkan/SwapchainInfo.cpp
	Source/Vulkan/ShaderModule.cpp
	Source/Vulkan/VertexBuffer.cpp
	Source/Vulkan/Buffer.cpp
	Source/Vulkan/Image.cpp
	Source/Vulkan/Texture.cpp
	Source/Vulkan/Util.cpp
	Source/Vulkan/ImageView.cpp
	Source/Vulkan/Swapchain.cpp
	Source/Vulkan/Sampler.cpp
	Source/Vulkan/DepthBuffer.cpp
	Source/Vulkan/Pipeline.cpp
	Source/Vulkan/Framebuffer.cpp
	Source/Vulkan/RenderPass.cpp
	Source/Vulkan/CommandBuffer.cpp
	# Vulkan Builders
	Source/Vulkan/Builders/PipelineBuilder.cpp
	Source/Vulkan/Builders/RenderPassBuilder.cpp
	Source/Vulkan/Builders/SubpassBuilder.cpp
	# Renderer sources
	Source/Renderer/RenderManager.cpp
	Source/Renderer/FreeCamera.cpp
	Source/Renderer/Camera.cpp
	# Render pipeline sources
	Source/Renderer/Pipelines/SwapPipeline.cpp
	Source/Renderer/Pipelines/ForwardPipeline.cpp
	# Render passes sources
	Source/Renderer/RenderPasses/SwapPass.cpp
	Source/Renderer/RenderPasses/ForwardPass.cpp
	# Model sources
	Source/Models/Mesh.cpp
	Source/Models/Vertex.cpp
	Source/Models/Model.cpp
    Source/Models/Material.cpp
	# External sources
	Source/Externals/GLM.cpp
	Source/Externals/STBImage.cpp
	Source/Externals/VMA.cpp
)

# Precompiled Headers
target_precompile_headers(VulkanRenderer PRIVATE
	# SDL headers
	<SDL2/SDL.h>
	<SDL2/SDL_vulkan.h>
	# Vulkan headers
	<vulkan/vulkan.h>
	<vulkan/vulkan_core.h>
	<vulkan/vk_enum_string_helper.h>
	# ImGui Headers
    Externals/ImGui/imgui.h
    Externals/ImGui/imgui_internal.h
    Externals/ImGui/backends/imgui_impl_sdl2.h
    Externals/ImGui/backends/imgui_impl_vulkan.h
    # STB Headers
    <stb/stb_image.h>
	# Assimp Headers
    <assimp/Importer.hpp>
    <assimp/scene.h>
    <assimp/postprocess.h>
    <assimp/types.h>
    # FMT Headers
    <fmt/core.h>
    <fmt/format.h>
    <fmt/chrono.h>
	<fmt/color.h>
    # GLM Headers
    <glm/vec2.hpp>
    <glm/vec3.hpp>
    <glm/vec4.hpp>
    <glm/mat4x4.hpp>
    <glm/ext/vector_int2.hpp>
    <glm/ext/vector_int4.hpp>
    <glm/ext/vector_uint3.hpp>
    <glm/trigonometric.hpp>
    <glm/geometric.hpp>
    <glm/gtc/matrix_transform.hpp>
    # VMA Headers
	Externals/VMA/include/vk_mem_alloc.h
    # C++ STL Headers
	<map>
    <unordered_map>
    <vector>
    <random>
    <filesystem>
    <array>
    <memory>
    <string_view>
    <string>
    <fstream>
    <chrono>
    <sstream>
	<span>
	<optional>
	<set>
	<tuple>
	<functional>
	<ranges>
	<istream>
	<source_location>
	<iostream>
	<numbers>
    # C STL Headers
    <cstdint>
    <cstddef>
	<cstdlib>
    <cassert>
    <cmath>
)

# Library includes
target_include_directories(VulkanRenderer PRIVATE Source/ Externals/)
# -O3 optimisations (Set to -Og when debugging)
target_compile_options(VulkanRenderer PRIVATE -Og)
# Enable all warnings
target_compile_options(VulkanRenderer PRIVATE -Wall -Wextra -Wpedantic)
# Enable CPU vectorisation (SSE or better)
target_compile_options(VulkanRenderer PRIVATE -march=native)

# Debug config
if (CMAKE_BUILD_TYPE STREQUAL Debug)
	# Engine Debug Mode
	target_compile_definitions(VulkanRenderer PRIVATE ENGINE_DEBUG)
	# Enable debug output
	target_compile_options(VulkanRenderer PRIVATE -g)
endif()

# Add shader dependency
add_dependencies(VulkanRenderer Shaders)

# Link libraries
target_link_libraries(VulkanRenderer
	Vulkan::Vulkan
	SDL2::SDL2main
	SDL2::SDL2
	fmt::fmt
	assimp::assimp
	GPUOpen::VulkanMemoryAllocator
	imgui
)

# Install
install(
	TARGETS VulkanRenderer imgui
	ARCHIVE DESTINATION ${INSTALL_DIR}
	LIBRARY DESTINATION ${INSTALL_DIR}
	RUNTIME DESTINATION ${INSTALL_DIR}
)

# Copy required DLLs
install(FILES $<TARGET_RUNTIME_DLLS:VulkanRenderer> DESTINATION ${INSTALL_DIR})
install(FILES $<TARGET_RUNTIME_DLLS:imgui>          DESTINATION ${INSTALL_DIR})

# Copy other directories
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Assets/GFX DESTINATION ${INSTALL_DIR}/Assets/)