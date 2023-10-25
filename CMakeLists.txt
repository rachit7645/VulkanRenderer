cmake_minimum_required(VERSION 3.25)
project(VulkanRenderer)

# Output executable/libraries to main build directory
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# C++ 20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(SDL2   REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glm    REQUIRED)
find_package(assimp REQUIRED)
find_package(fmt    REQUIRED)

# Add sub-projects
add_subdirectory(Externals/)

# Main executable
add_executable(VulkanRenderer
    # Main sources
	Source/Main.cpp
	# Engine sources
	Source/Engine/AppInstance.cpp
	Source/Engine/Window.cpp
    # Utility Sources
	Source/Util/Files.cpp
	Source/Util/Inputs.cpp
	Source/Util/Time.cpp
	# Vulkan source
	Source/Vulkan/Extensions.cpp
	Source/Vulkan/ValidationLayers.cpp
	Source/Vulkan/Context.cpp
	Source/Vulkan/QueueFamilyIndices.cpp
	# External sources
	Source/Externals/GLM.cpp
)

# Precompiled Headers
target_precompile_headers(VulkanRenderer PRIVATE
	# SDL headers
	<SDL2/SDL.h>
	<SDL2/SDL_vulkan.h>
	# ImGui Headers
    Externals/imgui/imgui.h
    Externals/imgui/imgui_internal.h
    Externals/imgui/backends/imgui_impl_sdl2.h
    Externals/imgui/backends/imgui_impl_opengl3.h
    Externals/imgui/misc/cpp/imgui_stdlib.h
    # STB Headers
    Externals/stb/stb_image.h
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
    # SIMD JSON Headers
    Externals/simdjson/singleheader/simdjson.h
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
    # C STL Headers
    <cstdint>
    <cstddef>
	<cstdlib>
    <cassert>
    <cmath>
)

# Library includes
target_include_directories(VulkanRenderer PRIVATE External/)
# Define _DEBUG in debug mode
target_compile_definitions(VulkanRenderer PRIVATE _DEBUG)
# Enable all warnings
target_compile_options(VulkanRenderer PRIVATE -Wall -Wextra -Wpedantic)
# -O3 optimisations
target_compile_options(VulkanRenderer PRIVATE -O3)
# Enable debug output
target_compile_options(VulkanRenderer PRIVATE -g)
# Enable CPU vectorisation (SSE or better)
target_compile_options(VulkanRenderer PRIVATE -march=native)
# Link libraries
target_link_libraries(VulkanRenderer Vulkan::Vulkan SDL2::SDL2main SDL2::SDL2 fmt::fmt assimp imgui simdjson)