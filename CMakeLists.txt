# Copyright 2023 - 2025 Rachit
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.25)
project(VulkanRenderer)

# Install location
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(INSTALL_DIR ${CMAKE_SOURCE_DIR}/Bin)

# C++ 20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(SDL3     REQUIRED COMPONENTS SDL3-shared CONFIG REQUIRED)
find_package(Vulkan   REQUIRED)
find_package(glm      REQUIRED)
find_package(fastgltf REQUIRED)
find_package(simdjson REQUIRED)
find_package(fmt      REQUIRED)
find_package(KTX      REQUIRED)

include(CheckIPOSupported)
check_ipo_supported(RESULT LTOSupported OUTPUT LTOError)

if (LTOSupported AND CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

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
    Source/Engine/Scene.cpp
    Source/Engine/Config.cpp
    # Utility Sources
	Source/Util/Files.cpp
	Source/Util/Time.cpp
	Source/Util/DeletionQueue.cpp
	Source/Util/FrameCounter.cpp
	Source/Util/Maths.cpp
    Source/Util/SIMD.cpp
    Source/Util/AABB.cpp
    Source/Util/Plane.cpp
    Source/Util/JSON.cpp
	# Vulkan sources
	Source/Vulkan/Extensions.cpp
	Source/Vulkan/DebugCallback.cpp
	Source/Vulkan/Context.cpp
	Source/Vulkan/QueueFamilyIndices.cpp
	Source/Vulkan/SwapchainInfo.cpp
	Source/Vulkan/ShaderModule.cpp
	Source/Vulkan/Buffer.cpp
	Source/Vulkan/Image.cpp
	Source/Vulkan/Texture.cpp
	Source/Vulkan/Util.cpp
	Source/Vulkan/ImageView.cpp
	Source/Vulkan/Swapchain.cpp
	Source/Vulkan/Sampler.cpp
	Source/Vulkan/Pipeline.cpp
	Source/Vulkan/CommandBuffer.cpp
	Source/Vulkan/DescriptorWriter.cpp
	Source/Vulkan/TextureManager.cpp
	Source/Vulkan/GeometryBuffer.cpp
    Source/Vulkan/MegaSet.cpp
    Source/Vulkan/DebugUtils.cpp
    Source/Vulkan/FormatHelper.cpp
    Source/Vulkan/FramebufferManager.cpp
    Source/Vulkan/AccelerationStructure.cpp
    Source/Vulkan/ShaderBindingTable.cpp
    Source/Vulkan/DescriptorAllocator.cpp
    Source/Vulkan/CommandBufferAllocator.cpp
    Source/Vulkan/ImmediateSubmit.cpp
    Source/Vulkan/Timeline.cpp
    Source/Vulkan/ImageUploader.cpp
    Source/Vulkan/PipelineBuilder.cpp
    Source/Vulkan/BarrierWriter.cpp
    Source/Vulkan/VertexBuffer.cpp
	# Renderer sources
	Source/Renderer/RenderManager.cpp
    # Object sources
    Source/Renderer/Objects/FreeCamera.cpp
    Source/Renderer/Objects/Camera.cpp
    # Renderer Buffer sources
    Source/Renderer/Buffers/IndirectBuffer.cpp
    Source/Renderer/Buffers/MeshBuffer.cpp
    Source/Renderer/Buffers/SceneBuffer.cpp
    Source/Renderer/Buffers/LightsBuffer.cpp
    Source/Renderer/Buffers/DrawCallBuffer.cpp
	# Post Process Pass Sources
	Source/Renderer/PostProcess/Pipeline.cpp
	Source/Renderer/PostProcess/RenderPass.cpp
    # Depth Pass Sources
    Source/Renderer/Depth/Pipeline.cpp
    Source/Renderer/Depth/RenderPass.cpp
    # ImGui Pass Sources
    Source/Renderer/ImGui/Pipeline.cpp
    Source/Renderer/ImGui/RenderPass.cpp
    # Skybox Pass Sources
    Source/Renderer/Skybox/Pipeline.cpp
    Source/Renderer/Skybox/RenderPass.cpp
    # IBL Sources
    Source/Renderer/IBL/IBLMaps.cpp
    Source/Renderer/IBL/Generator.cpp
    Source/Renderer/IBL/BRDF/Pipeline.cpp
    Source/Renderer/IBL/Converter/Pipeline.cpp
    Source/Renderer/IBL/Convolution/Pipeline.cpp
    Source/Renderer/IBL/PreFilter/Pipeline.cpp
    # Bloom Pass sources
    Source/Renderer/Bloom/RenderPass.cpp
    Source/Renderer/Bloom/DownSample/Pipeline.cpp
    Source/Renderer/Bloom/UpSample/Pipeline.cpp
    # Point Shadow Pass Sources
    Source/Renderer/PointShadow/Pipeline.cpp
    Source/Renderer/PointShadow/RenderPass.cpp
    # Point Shadow Pass Sources
    Source/Renderer/SpotShadow/Pipeline.cpp
    Source/Renderer/SpotShadow/RenderPass.cpp
    # Culling Dispatch Sources
    Source/Renderer/Culling/Pipeline.cpp
    Source/Renderer/Culling/Dispatch.cpp
    Source/Renderer/Culling/FrustumBuffer.cpp
    # GBuffer Pass Sources
    Source/Renderer/GBuffer/Pipeline.cpp
    Source/Renderer/GBuffer/RenderPass.cpp
    # Lighting Pass Sources
    Source/Renderer/Lighting/Pipeline.cpp
    Source/Renderer/Lighting/RenderPass.cpp
    # VBGTAO Pass Sources
    Source/Renderer/AO/VBGTAO/RenderPass.cpp
    Source/Renderer/AO/VBGTAO/DepthPreFilter/Pipeline.cpp
    Source/Renderer/AO/VBGTAO/Occlusion/Pipeline.cpp
    Source/Renderer/AO/VBGTAO/Denoise/Pipeline.cpp
    # Shadow RT sources
    Source/Renderer/ShadowRT/Pipeline.cpp
    Source/Renderer/ShadowRT/RenderPass.cpp
    # TAA Sources
    Source/Renderer/TAA/Pipeline.cpp
    Source/Renderer/TAA/RenderPass.cpp
	# Model sources
	Source/Models/Model.cpp
	Source/Models/ModelManager.cpp
	# External sources
	Source/Externals/GLM.cpp
	Source/Externals/VMA.cpp
    Source/Externals/STB.cpp
)

# Precompiled Headers
target_precompile_headers(VulkanRenderer PRIVATE
	# SDL headers
	<SDL3/SDL.h>
	<SDL3/SDL_vulkan.h>
	# Vulkan headers
	<vulkan/vulkan.h>
	<vulkan/vulkan_core.h>
	<vulkan/vk_enum_string_helper.h>
    Externals/volk/volk.h
	# ImGui Headers
    Externals/imgui/imgui.h
    Externals/imgui/imgui_internal.h
    Externals/imgui/backends/imgui_impl_sdl2.h
    Externals/imgui/backends/imgui_impl_vulkan.h
    # KTX Headers
    <ktx.h>
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
	Externals/vma/include/vk_mem_alloc.h
    # fastgltf Headers
    <fastgltf/core.hpp>
    <fastgltf/types.hpp>
    <fastgltf/tools.hpp>
    <fastgltf/glm_element_traits.hpp>
    <fastgltf/math.hpp>
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
	<deque>
    <variant>
    <list>
    <queue>
    # C STL Headers
    <cstdint>
    <cstddef>
	<cstdlib>
    <cassert>
    <cmath>
)

# Library includes
target_include_directories(VulkanRenderer PRIVATE Source/ Externals/)
# No vulkan prototypes
target_compile_definitions(VulkanRenderer PRIVATE VK_NO_PROTOTYPES)
# -O3 optimisations (Set to -Og when debugging)
target_compile_options(VulkanRenderer PRIVATE -O3)
# Enable all warnings
target_compile_options(VulkanRenderer PRIVATE -Wall -Wextra -Wpedantic)
# Enable CPU vectorisation (SSE or better)
target_compile_options(VulkanRenderer PRIVATE -msse2 -mavx -mavx2 -mf16c)

# Debug config
if (CMAKE_BUILD_TYPE STREQUAL Debug)
	# Engine Debug Mode
	target_compile_definitions(VulkanRenderer PRIVATE ENGINE_DEBUG)
	# Enable debug output
	target_compile_options(VulkanRenderer PRIVATE -g)
endif()

# Release config
if (CMAKE_BUILD_TYPE STREQUAL Release)
    # Release Mode
    target_compile_definitions(VulkanRenderer PRIVATE NDEBUG)
endif()

# Add shader dependency
add_dependencies(VulkanRenderer Shaders)

# Link libraries
target_link_libraries(VulkanRenderer
    Vulkan::Headers
    volk::volk
	SDL3::SDL3
	fmt::fmt
	fastgltf::fastgltf
    simdjson::simdjson
	GPUOpen::VulkanMemoryAllocator
    KTX::ktx
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

# Copy other directories
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Assets/GFX    DESTINATION ${INSTALL_DIR}/Assets)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/Assets/Scenes DESTINATION ${INSTALL_DIR}/Assets)

# Copy misc files
install(FILES ${CMAKE_SOURCE_DIR}/Assets/Config.json DESTINATION ${INSTALL_DIR}/Assets)