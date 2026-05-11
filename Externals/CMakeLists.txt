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

# Check if LTO is supported
include(CheckIPOSupported)
check_ipo_supported(RESULT LTOSupported OUTPUT LTOError)

if (MSVC)
    add_compile_options(/arch:AVX2)
else()
    add_compile_options(-mavx2 -mf16c)
endif()

# Install location
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(ENGINE_INSTALL_DIR   ${CMAKE_SOURCE_DIR}/Bin)

# C++23 is required
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build shared libraries
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

function(engine_add_external directory)
    # Parse optional arguments
    set(options EXCLUDE_FROM_ALL ENABLE_LTO)
    set(oneValueArgs INSTALL_TARGET)
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")

    # Add subdirectory
    if (ARG_EXCLUDE_FROM_ALL)
        add_subdirectory(${directory} EXCLUDE_FROM_ALL)
    else()
        add_subdirectory(${directory})
    endif()

    # Enable LTO
    if (ARG_ENABLE_LTO AND ARG_INSTALL_TARGET)
        if (LTOSupported)
            # Get the actual target if it's an ALIAS
            get_target_property(AliasedTarget ${ARG_INSTALL_TARGET} ALIASED_TARGET)

            if (AliasedTarget)
                set(ActualTarget ${AliasedTarget})
            else()
                set(ActualTarget ${ARG_INSTALL_TARGET})
            endif()

            set_target_properties(${ActualTarget} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()

    # Handle manual installation
    if (ARG_INSTALL_TARGET)
        install(
            FILES $<TARGET_FILE:${ARG_INSTALL_TARGET}>
            DESTINATION ${ENGINE_INSTALL_DIR}
            COMPONENT Runtime
        )

        install(
            FILES $<TARGET_RUNTIME_DLLS:${ARG_INSTALL_TARGET}>
            DESTINATION ${ENGINE_INSTALL_DIR}
            COMPONENT Runtime
        )
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
set(FMT_MODULE  OFF CACHE BOOL "Disable fmt modules"      FORCE)

# fastgltf Config
set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "Use C++20" FORCE)

# OpenEXR Config
set(OPENEXR_INSTALL        OFF CACHE BOOL "Disable openexr installation"       FORCE)
set(OPENEXR_BUILD_TOOLS    OFF CACHE BOOL "Disable building openexr tools"     FORCE)
set(OPENEXR_BUILD_EXAMPLES OFF CACHE BOOL "Disable building openexr examples"  FORCE)
set(OPENEXR_INSTALL_TOOLS  OFF CACHE BOOL "Disable openexr tools installation" FORCE)

# External Libraries
engine_add_external(vma                                                                          )
engine_add_external(volk                                                                         )
engine_add_external(unordered_dense                                                              )
engine_add_external(glm                              ENABLE_LTO INSTALL_TARGET glm::glm          )
engine_add_external(fmt                              ENABLE_LTO INSTALL_TARGET fmt::fmt          )
engine_add_external(sdl             EXCLUDE_FROM_ALL            INSTALL_TARGET SDL3::SDL3-shared )
engine_add_external(simdjson        EXCLUDE_FROM_ALL            INSTALL_TARGET simdjson::simdjson)
engine_add_external(fastgltf        EXCLUDE_FROM_ALL            INSTALL_TARGET fastgltf::fastgltf)
engine_add_external(openexr         EXCLUDE_FROM_ALL ENABLE_LTO INSTALL_TARGET OpenEXR::OpenEXR  )

# Vulkan Headers
target_link_libraries(volk PRIVATE Vulkan::Headers)

# Profile config
if (CMAKE_BUILD_TYPE STREQUAL Profile)
    # Tracy Config
    set(TRACY_ENABLE         ON CACHE BOOL "Enable Tracy"         FORCE)
    set(TRACY_ON_DEMAND      ON CACHE BOOL "On-Demand Mode"       FORCE)
    set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "Localhost Only Mode"  FORCE)
    set(TRACY_CALLSTACK      ON CACHE BOOL "Enable Callstack"     FORCE)

    # Add Tracy (Requires v0.11.1!)
    engine_add_external(tracy EXCLUDE_FROM_ALL INSTALL_TARGET TracyClient)
endif()

# ImGui source files
add_library(DearImGui STATIC
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_sdl3.cpp
	imgui/misc/cpp/imgui_stdlib.cpp
    implot/implot.cpp
    implot/implot_items.cpp
    # imgui/imgui_demo.cpp
    # implot/implot_demo.cpp
)

# ImGui precompiled headers
target_precompile_headers(DearImGui PRIVATE
    imgui/imconfig.h
    imgui/imgui_internal.h
    imgui/imgui.h
    imgui/imstb_rectpack.h
    imgui/imstb_truetype.h
    imgui/backends/imgui_impl_sdl3.h
    implot/implot.h
    implot/implot_internal.h
)

# SDL2 Includes
target_include_directories(DearImGui PRIVATE sdl/include/)
# ImGUI Includes
target_include_directories(DearImGui PUBLIC imgui/ imgui/backends imgui/misc implot/)
# ImGui Defines
target_compile_definitions(DearImGui PRIVATE IMGUI_DEFINE_MATH_OPERATORS=1 ImTextureID_Invalid=0xFFFFFFFFFFFFFFFFull)

# Compiler options
if (MSVC)
    target_compile_options(DearImGui PRIVATE /w /O2)
    target_compile_definitions(DearImGui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(DearImGui PRIVATE -w -O3)
endif()

# Link libraries
target_link_libraries(DearImGui SDL3::SDL3-shared)

# DLSS SDK will only build on MSVC-Style Windows Compilers and Linux
if (WIN32)
    if (MSVC)
        set(DLSSAvailable ON)
    else()
        set(DLSSAvailable OFF)
    endif()
else()
    set(DLSSAvailable ON)
endif()

# You can use this to forcefully disable DLSS
# set(DLSSAvailable OFF)

set(DLSSAvailable ${DLSSAvailable} PARENT_SCOPE)

if (DLSSAvailable)
    # DLSS
    set(DLSS_ROOT ${CMAKE_SOURCE_DIR}/Externals/dlss)

    # DLSS Paths
    if (WIN32)
        # Debug Mode DLL
        # set(DLSS_DLL "${DLSS_ROOT}/lib/Windows_x86_64/dev/nvngx_dlss.dll")

        set(DLSS_DLL "${DLSS_ROOT}/lib/Windows_x86_64/rel/nvngx_dlss.dll")
        set(DLSS_LIB "${DLSS_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib")
    else()
        set(DLSS_DLL "${DLSS_ROOT}/lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.310.5.3")
        set(DLSS_LIB "${DLSS_ROOT}/lib/Linux_x86_64/libnvsdk_ngx.a")
    endif()

    # DLSS Imported Library
    add_library(DLSS SHARED IMPORTED GLOBAL)

    # Setup DLSS import
    set_target_properties(DLSS PROPERTIES
        IMPORTED_IMPLIB   "${DLSS_LIB}"
        IMPORTED_LOCATION "${DLSS_DLL}"
    )

    # Install DLSS
    install(IMPORTED_RUNTIME_ARTIFACTS DLSS)
endif()

# lz4 source files
add_library(lz4 SHARED
    lz4/lib/lz4.c
    lz4/lib/lz4hc.c
)

# lz4 precompiled headers
target_precompile_headers(lz4 PRIVATE
    lz4/lib/lz4.h
    lz4/lib/lz4hc.h
)

# Standard options
target_compile_definitions(lz4 PRIVATE LZ4_DLL_EXPORT=1)

# Compiler options
if (MSVC)
    target_compile_options(lz4 PRIVATE /w /O2)
    target_compile_definitions(lz4 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(lz4 PRIVATE -w -O3)
endif()

# Install
install(
    FILES $<TARGET_FILE:lz4>
    DESTINATION ${ENGINE_INSTALL_DIR}
    COMPONENT Runtime
)