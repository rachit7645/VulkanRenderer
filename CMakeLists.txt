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
set(ENGINE_INSTALL_DIR   ${CMAKE_SOURCE_DIR}/Bin)

# C++23 is required
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(Vulkan REQUIRED)
find_package(KTX    REQUIRED)

# Check for LTO
include(CheckIPOSupported)
check_ipo_supported(RESULT LTOSupported OUTPUT LTOError)

# Only GCC/Clang are supported
if (MSVC)
    message(FATAL_ERROR "MSVC is not supported!")
endif()

# Force clang color output
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

# Sub-directories
add_subdirectory(Externals)
add_subdirectory(Assets)

# Main executable
add_executable(VulkanRenderer
    # Main sources
	Source/Main.cpp
	# Engine sources
	Source/Engine/Application.cpp
	Source/Engine/Window.cpp
	Source/Engine/Inputs.cpp
    Source/Engine/Config.cpp
    Source/Engine/Cache.cpp
    Source/Engine/Camera.cpp
    Source/Engine/FreeCamera.cpp
    Source/Engine/SceneEditor.cpp
    # Utility Sources
	Source/Util/Files.cpp
	Source/Util/Time.cpp
	Source/Util/DeletionQueue.cpp
	Source/Util/FrameCounter.cpp
	Source/Util/Maths.cpp
    Source/Util/SIMD.cpp
    Source/Util/JSON.cpp
    Source/Util/String.cpp
    Source/Util/Threads.cpp
    Source/Util/WireframeSphere.cpp
    Source/Util/Debug.cpp
    Source/Util/WireframeCone.cpp
	# Vulkan sources
	Source/Vulkan/Extensions.cpp
	Source/Vulkan/DebugCallback.cpp
	Source/Vulkan/Context.cpp
	Source/Vulkan/QueueFamilies.cpp
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
    Source/Vulkan/GraphicsTimeline.cpp
    Source/Vulkan/ImageUploader.cpp
    Source/Vulkan/PipelineConfig.cpp
    Source/Vulkan/BarrierWriter.cpp
    Source/Vulkan/IndexBuffer.cpp
    Source/Vulkan/ComputeTimeline.cpp
    Source/Vulkan/PipelineManager.cpp
    Source/Vulkan/MemoryBlock.cpp
    Source/Vulkan/StagingPool.cpp
    Source/Vulkan/Properties.cpp
    Source/Vulkan/ImageDownloader.cpp
    Source/Vulkan/SamplerID.cpp
    Source/Vulkan/TextureID.cpp
    Source/Vulkan/VertexBuffer.cpp
	# Renderer sources
	Source/Renderer/RenderManager.cpp
    Source/Renderer/RenderConfig.cpp
    # Renderer Util sources
    Source/Renderer/Util/Jitter.cpp
    # Object sources
    Source/Renderer/Objects/Samplers.cpp
    # Renderer Buffer sources
    Source/Renderer/Buffers/IndirectBuffer.cpp
    Source/Renderer/Buffers/MeshBuffer.cpp
    Source/Renderer/Buffers/SceneBuffer.cpp
    Source/Renderer/Buffers/DrawCallBuffer.cpp
    Source/Renderer/Buffers/TileLightIndexBuffer.cpp
    Source/Renderer/Buffers/ExposureBuffers.cpp
	# Graphics Pass Sources
	Source/Renderer/Tonemap/RenderPass.cpp
    Source/Renderer/Depth/RenderPass.cpp
    Source/Renderer/ImGui/RenderPass.cpp
    Source/Renderer/Skybox/RenderPass.cpp
    Source/Renderer/Bloom/RenderPass.cpp
    Source/Renderer/PointShadow/RenderPass.cpp
    Source/Renderer/GBuffer/RenderPass.cpp
    Source/Renderer/Lighting/RenderPass.cpp
    Source/Renderer/TAA/RenderPass.cpp
    Source/Renderer/SpotShadow/RenderPass.cpp
    Source/Renderer/Debug/RenderPass.cpp
    # Compute Pass Sources
    Source/Renderer/Culling/Dispatch.cpp
    Source/Renderer/Culling/FrustumBuffer.cpp
    Source/Renderer/AO/VBAO/Dispatch.cpp
    Source/Renderer/TiledLighting/Dispatch.cpp
    Source/Renderer/Exposure/Dispatch.cpp
    # Ray Tracing Pass Sources
    Source/Renderer/ShadowRT/RayDispatch.cpp
    # IBL Sources
    Source/Renderer/IBL/IBLMaps.cpp
    Source/Renderer/IBL/Generator.cpp
	# Model sources
	Source/Models/Model.cpp
	Source/Models/ModelManager.cpp
    Source/Models/Mesh.cpp
    Source/Models/Material.cpp
    Source/Models/MeshIdentifier.cpp
    Source/Models/LocalLights.cpp
	# External sources
	Source/Externals/GLM.cpp
	Source/Externals/VMA.cpp
    Source/Externals/STB.cpp
    Source/Externals/FastGLTF.cpp
    Source/Externals/SDL.cpp
)

# Precompiled Headers
target_precompile_headers(VulkanRenderer PRIVATE
    # SDL headers
    Externals/sdl/include/SDL3/SDL.h
    Externals/sdl/include/SDL3/SDL_vulkan.h
    # Vulkan headers
    <vulkan/vulkan.h>
    <vulkan/vulkan_core.h>
    <vulkan/vk_enum_string_helper.h>
    Externals/volk/volk.h
    # ImGui Headers
    Externals/imgui/imgui.h
    Externals/imgui/backends/imgui_impl_sdl3.h
    Externals/implot/implot.h
    # KTX Headers
    <ktx.h>
    # FMT Headers
    Externals/fmt/include/fmt/core.h
    Externals/fmt/include/fmt/format.h
    Externals/fmt/include/fmt/chrono.h
    Externals/fmt/include/fmt/color.h
    # GLM Headers
    Externals/glm/glm/vec2.hpp
    Externals/glm/glm/vec3.hpp
    Externals/glm/glm/vec4.hpp
    Externals/glm/glm/mat4x4.hpp
    Externals/glm/glm/ext/vector_int2.hpp
    Externals/glm/glm/ext/vector_int4.hpp
    Externals/glm/glm/ext/vector_uint2.hpp
    Externals/glm/glm/ext/vector_uint3.hpp
    Externals/glm/glm/trigonometric.hpp
    Externals/glm/glm/geometric.hpp
    Externals/glm/glm/vector_relational.hpp
    Externals/glm/glm/gtc/type_ptr.hpp
    Externals/glm/glm/gtc/matrix_transform.hpp
    # VMA Headers
    Externals/vma/include/vk_mem_alloc.h
    # simdjson Headers
    Externals/simdjson/include/simdjson.h
    # fastgltf Headers
    Externals/fastgltf/include/fastgltf/core.hpp
    Externals/fastgltf/include/fastgltf/types.hpp
    Externals/fastgltf/include/fastgltf/tools.hpp
    Externals/fastgltf/include/fastgltf/glm_element_traits.hpp
    Externals/fastgltf/include/fastgltf/math.hpp
    # Taskflow Headers
    Externals/taskflow/taskflow/taskflow.hpp
    # STB Headers
    Externals/stb/stb_image.h
    # OpenEXR Headers
    Externals/openexr/src/lib/OpenEXR/ImfRgbaFile.h
    Externals/openexr/src/lib/OpenEXR/ImfArray.h
    # Unordered Dense Headers
    Externals/unordered_dense/include/ankerl/unordered_dense.h
    # lz4 Headers
    Externals/lz4/lib/lz4.h
    Externals/lz4/lib/lz4hc.h
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
    <stacktrace>
    # C STL Headers
    <cstdint>
    <cstddef>
    <cstdlib>
    <cassert>
    <cmath>
)

# Library includes
target_include_directories(VulkanRenderer PRIVATE Source/ Externals/ Externals/taskflow/ Shared/)
# No Vulkan prototypes
target_compile_definitions(VulkanRenderer PRIVATE VK_NO_PROTOTYPES ImTextureID_Invalid=0xFFFFFFFFFFFFFFFFull)

# Compiler options
target_compile_options(VulkanRenderer PRIVATE -O3)
target_compile_options(VulkanRenderer PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(VulkanRenderer PRIVATE -mavx2 -mf16c -mpclmul -mbmi -mbmi2 -mlzcnt)

# Windows options
if (WIN32)
    target_compile_definitions(VulkanRenderer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Debug config
if (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
	# Engine Debug Mode
	target_compile_definitions(VulkanRenderer PRIVATE ENGINE_DEBUG)
endif()

# Release config
if (CMAKE_BUILD_TYPE STREQUAL Release)
    # Enable LTO
    if (LTOSupported)
        set_property(TARGET VulkanRenderer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    # Release Mode
    target_compile_definitions(VulkanRenderer PRIVATE NDEBUG)
endif()

# Profile config
if (CMAKE_BUILD_TYPE STREQUAL Profile)
    # Enable LTO
    if (LTOSupported)
        set_property(TARGET VulkanRenderer PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()

    # Engine Debug Mode
    target_compile_definitions(VulkanRenderer PRIVATE ENGINE_DEBUG)
    # Engine Profile Mode
    target_compile_definitions(VulkanRenderer PRIVATE ENGINE_PROFILE TRACY_ENABLE)

    # Detailed Callstack
    # target_compile_definitions(VulkanRenderer PRIVATE TRACY_CALLSTACK=32)

    # Enable debug symbols
    target_compile_options(VulkanRenderer PRIVATE -g)

    # Add Tracy
    target_link_libraries(VulkanRenderer TracyClient)
    # Add Tracy Headers
    target_precompile_headers(VulkanRenderer PRIVATE
        Externals/tracy/public/tracy/Tracy.hpp
        Externals/tracy/public/tracy/TracyVulkan.hpp
    )
endif()

# Add shader dependency
add_dependencies(VulkanRenderer Shaders)

# Link libraries
target_link_libraries(VulkanRenderer
    Vulkan::Headers
    volk::volk
    SDL3::SDL3-shared
	fmt::fmt
	fastgltf::fastgltf
    simdjson::simdjson
	GPUOpen::VulkanMemoryAllocator
    KTX::ktx
    glm::glm
    OpenEXR::OpenEXR
    unordered_dense::unordered_dense
    libzstd_shared
    DearImGui
    lz4
)

if (DLSSAvailable)
    # DLSS Sources
    target_sources(VulkanRenderer PRIVATE
        Source/Renderer/DLSS/DLSSConfig.cpp
        Source/Renderer/DLSS/Evaluation.cpp
        Source/Externals/DLSS.cpp
    )
    # Mark DLSS As Available
    target_compile_definitions(VulkanRenderer PRIVATE ENGINE_DLSS)
    # DLSS Precompiled Headers
    target_precompile_headers(VulkanRenderer PRIVATE
        Externals/dlss/include/nvsdk_ngx.h
        Externals/dlss/include/nvsdk_ngx_defs.h
        Externals/dlss/include/nvsdk_ngx_params.h
        Externals/dlss/include/nvsdk_ngx_helpers.h
        Externals/dlss/include/nvsdk_ngx_vk.h
        Externals/dlss/include/nvsdk_ngx_helpers_vk.h
    )
    # Link DLSS
    target_link_libraries(VulkanRenderer DLSS)
endif()

# Install
install(
	TARGETS VulkanRenderer
    ARCHIVE DESTINATION ${ENGINE_INSTALL_DIR}
	LIBRARY DESTINATION ${ENGINE_INSTALL_DIR}
	RUNTIME DESTINATION ${ENGINE_INSTALL_DIR}
)

# Store application name at config time
set(APPLICATION_NAME "$<TARGET_FILE_NAME:VulkanRenderer>")

# Copy required DLLs
install(CODE "
    include(BundleUtilities)
    fixup_bundle(
        \"${ENGINE_INSTALL_DIR}/${APPLICATION_NAME}\"
        \"\"
        \"\"
    )
")