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
project(Externals)

# C++ 20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(SDL3   REQUIRED COMPONENTS SDL3-shared CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

option(VOLK_PULL_IN_VULKAN OFF)
option(VOLK_INSTALL        ON)

# Libraries with CMake support
add_subdirectory(vma/)
add_subdirectory(volk/)

# ImGui source files
add_library(imgui STATIC
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	# imgui/imgui_demo.cpp
	imgui/backends/imgui_impl_sdl3.cpp
	imgui/backends/imgui_impl_vulkan.cpp
	imgui/misc/cpp/imgui_stdlib.cpp
)

# ImGui precompiled headers
target_precompile_headers(imgui PRIVATE
	imgui/imconfig.h
	imgui/imgui_internal.h
	imgui/imgui.h
	imgui/imstb_rectpack.h
	imgui/imstb_truetype.h
	imgui/backends/imgui_impl_sdl3.h
	imgui/backends/imgui_impl_vulkan.h
)

# SDL2 Includes
target_include_directories(imgui PRIVATE ${SDL3_INCLUDE_DIRS})
# ImGUI Includes
target_include_directories(imgui PUBLIC imgui/ imgui/backends imgui/misc)
# ImGui Defines
target_compile_definitions(imgui PRIVATE IMGUI_DEFINE_MATH_OPERATORS=1)
# Volk setup
# No vulkan prototypes
target_compile_definitions(imgui PRIVATE VK_NO_PROTOTYPES)
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
# Disable warnings
target_compile_options(imgui PRIVATE -w)
# -O3 optimisations
target_compile_options(imgui PRIVATE -O3)
# Enable CPU vectorisation (SSE or better)
target_compile_options(imgui PRIVATE -march=native)

# Debug config
if (CMAKE_BUILD_TYPE STREQUAL Debug)
	# Enable debug output
	target_compile_options(imgui PRIVATE -g)
endif()

# Debug Without Validation config
if (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    # Enable debug output
    target_compile_options(imgui PRIVATE -g)
endif()

# Link libraries
target_link_libraries(imgui Vulkan::Vulkan SDL3::SDL3 volk::volk)