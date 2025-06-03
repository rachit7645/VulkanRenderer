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

# Install location
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(ENGINE_INSTALL_DIR   ${CMAKE_SOURCE_DIR}/Bin)

# C++ 20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Save current flags
set(ORIGINAL_C_FLAGS   ${CMAKE_C_FLAGS})
set(ORIGINAL_CXX_FLAGS ${CMAKE_CXX_FLAGS})

# Force Release flags
set(CMAKE_C_FLAGS   ${CMAKE_C_FLAGS_RELEASE})
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS_RELEASE})

# Build shared libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

function(engine_add_external directory)
    # Parse optional arguments
    set(options EXCLUDE_FROM_ALL FORCE_RELEASE)
    set(oneValueArgs INSTALL_TARGET)
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")

    # Save current build type
    set(ORIGINAL_BUILD_TYPE ${CMAKE_BUILD_TYPE})

    # Force Release
    if(ARG_FORCE_RELEASE)
        set(CMAKE_BUILD_TYPE Release)
    endif()

    # Add subdirectory
    if(ARG_EXCLUDE_FROM_ALL)
        add_subdirectory(${directory} EXCLUDE_FROM_ALL)
    else()
        add_subdirectory(${directory})
    endif()

    # Handle manual installation
    if(ARG_INSTALL_TARGET)
        install(
            FILES $<TARGET_FILE:${ARG_INSTALL_TARGET}>
            DESTINATION ${ENGINE_INSTALL_DIR}
            COMPONENT Runtime
        )
    endif()

    # Restore original build type
    if(ARG_FORCE_RELEASE)
        set(CMAKE_BUILD_TYPE ${ORIGINAL_BUILD_TYPE})
    endif()
endfunction()

# Required packages
find_package(Vulkan REQUIRED)

# Volk Config
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "Disable pulling in Vulkan in volk" FORCE)
set(VOLK_INSTALL        OFF CACHE BOOL "Disable volk installation"         FORCE)

# GLM Config
set(GLM_ENABLE_CXX_20    ON CACHE BOOL "Use C++20"   FORCE)
set(GLM_ENABLE_SIMD_AVX2 ON CACHE BOOL "Enable AVX2" FORCE)

# fmt Config
set(FMT_INSTALL OFF CACHE BOOL "Disable fmt installation" FORCE)

# fastgltf Config
set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "Use C++20" FORCE)

# External Libraries
engine_add_external(vma      FORCE_RELEASE                                                   )
engine_add_external(volk     FORCE_RELEASE                                                   )
engine_add_external(glm      FORCE_RELEASE                  INSTALL_TARGET glm::glm          )
engine_add_external(fmt                                     INSTALL_TARGET fmt::fmt          )
engine_add_external(sdl                    EXCLUDE_FROM_ALL INSTALL_TARGET SDL3::SDL3-shared )
engine_add_external(simdjson FORCE_RELEASE EXCLUDE_FROM_ALL INSTALL_TARGET simdjson::simdjson)
engine_add_external(fastgltf FORCE_RELEASE EXCLUDE_FROM_ALL INSTALL_TARGET fastgltf::fastgltf)

# Profile config
if (CMAKE_BUILD_TYPE STREQUAL Profile)
    # Tracy Config
    set(TRACY_ENABLE         ON CACHE BOOL "Enable Tracy"         FORCE)
    set(TRACY_ON_DEMAND      ON CACHE BOOL "On-Demand Mode"       FORCE)
    set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "Localhost Only Mode"  FORCE)
    set(TRACY_CALLSTACK      ON CACHE BOOL "Enable Callstack"     FORCE)

    # Add Tracy (Requires v0.11.1!)
    engine_add_external(tracy FORCE_RELEASE EXCLUDE_FROM_ALL INSTALL_TARGET TracyClient)
endif()

# ImGui source files
add_library(DearImGui SHARED
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_sdl3.cpp
	imgui/misc/cpp/imgui_stdlib.cpp
    # imgui/imgui_demo.cpp
)

# ImGui precompiled headers
target_precompile_headers(DearImGui PRIVATE
	imgui/imconfig.h
	imgui/imgui_internal.h
	imgui/imgui.h
	imgui/imstb_rectpack.h
	imgui/imstb_truetype.h
	imgui/backends/imgui_impl_sdl3.h
)

# SDL2 Includes
target_include_directories(DearImGui PRIVATE sdl/include/)
# ImGUI Includes
target_include_directories(DearImGui PUBLIC imgui/ imgui/backends imgui/misc)
# ImGui Defines
target_compile_definitions(DearImGui PRIVATE IMGUI_DEFINE_MATH_OPERATORS=1)
# Disable warnings
target_compile_options(DearImGui PRIVATE -w)
# -O3 optimisations
target_compile_options(DearImGui PRIVATE -O3)
# Enable CPU vectorisation
target_compile_options(DearImGui PRIVATE -mavx2)
# Link libraries
target_link_libraries(DearImGui SDL3::SDL3-shared)

# Install Dear ImGui
install(
    TARGETS DearImGui
    ARCHIVE DESTINATION ${ENGINE_INSTALL_DIR}
    LIBRARY DESTINATION ${ENGINE_INSTALL_DIR}
    RUNTIME DESTINATION ${ENGINE_INSTALL_DIR}
)

# Restore original flags
set(CMAKE_C_FLAGS   ${ORIGINAL_C_FLAGS})
set(CMAKE_CXX_FLAGS ${ORIGINAL_CXX_FLAGS})