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
project(Shaders)

# Install location
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(SHADERS_INSTALL_DIR  ${CMAKE_INSTALL_PREFIX}/Bin/Assets/Shaders)

# Get glsl compiler
find_package(Vulkan REQUIRED COMPONENTS glslangValidator)
find_program(GLSL_COMPILER NAMES glslangValidator HINTS Vulkan::glslangValidator)

# Shader locations
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SPIRV_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR})

# Shader flags
set(SHADER_FLAGS --target-env vulkan1.3 --enhanced-msgs)

# Debug config
if (CMAKE_BUILD_TYPE STREQUAL Debug)
    list(APPEND SHADER_FLAGS -gVS)
endif()

# Includes
set(SHADER_INCLUDES -I${SHADER_SOURCE_DIR}/Include -I${CMAKE_SOURCE_DIR}/Shared -I${CMAKE_SOURCE_DIR}/Shared/GPU)

# Shader files
file(GLOB SHADER_SOURCES
    Misc/PostProcess.frag
    Misc/TAA.frag
    Misc/Trongle.vert
    Misc/Empty.frag
    Culling/Frustum.comp
    ImGui/ImGui.vert
    ImGui/ImGui.frag
    IBL/BRDF.frag
    IBL/Converter.vert
    IBL/Converter.frag
    IBL/Convolution.frag
    IBL/Convolution.vert
    IBL/PreFilter.vert
    IBL/PreFilter.frag
    Skybox/Skybox.frag
    Skybox/Skybox.vert
    Bloom/DownSample/Regular.frag
    Bloom/DownSample/FirstSample.frag
    Bloom/UpSample.frag
    Shadows/PointShadow/Opaque.vert
    Shadows/PointShadow/Opaque.frag
    Shadows/PointShadow/AlphaMasked.vert
    Shadows/PointShadow/AlphaMasked.frag
    Shadows/RT/Shadow.rgen
    Shadows/RT/Shadow.rmiss
    Shadows/RT/Shadow.rahit
    Shadows/SpotShadow/Opaque.vert
    Shadows/SpotShadow/AlphaMasked.vert
    Shadows/SpotShadow/AlphaMasked.frag
    Deferred/Depth/Opaque.vert
    Deferred/Depth/AlphaMasked.vert
    Deferred/Depth/AlphaMasked.frag
    Deferred/GBuffer/GBuffer.vert
    Deferred/GBuffer/SingleSided.frag
    Deferred/GBuffer/DoubleSided.frag
    Deferred/Lighting.frag
    AO/VBGTAO/DepthPreFilter.comp
    AO/VBGTAO/SpacialDenoise.comp
    AO/VBGTAO/VBGTAO.comp
)

# Shader headers
file(GLOB_RECURSE SHADER_HEADERS
    ${SHADER_SOURCE_DIR}/Include/*.glsl
    ${SHADER_SOURCE_DIR}/Include/Constants/*.glsl
    ${CMAKE_SOURCE_DIR}/Shared/*.h
    ${CMAKE_SOURCE_DIR}/Shared/Include/*.h
)

# Ensure CMake reconfigures when new includes are added
set_property(GLOBAL PROPERTY CMAKE_CONFIGURE_DEPENDS ${SHADER_HEADERS})

# For each shader to be compiled
foreach(Shader ${SHADER_SOURCES})
    # Extract relative path within the project
    file(RELATIVE_PATH REL_PATH ${SHADER_SOURCE_DIR} ${Shader})
    get_filename_component(SPIRV_DIR "${SPIRV_BINARY_DIR}/${REL_PATH}" DIRECTORY)
    set(SPIRV "${SPIRV_BINARY_DIR}/${REL_PATH}.spv")
    # Ensure the necessary directory structure is created
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_DIR}"
        COMMAND ${GLSL_COMPILER} ${Shader} ${SHADER_FLAGS} ${SHADER_INCLUDES} -o ${SPIRV}
        DEPENDS ${Shader} ${SHADER_HEADERS}
        COMMENT "Building GLSL shader ${Shader}"
    )
    # Add compiled shader to list
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(Shader)

# Create custom shaders target
add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})

foreach(SPIRV_FILE ${SPIRV_BINARY_FILES})
    # Extract relative path for proper installation structure
    file(RELATIVE_PATH REL_PATH ${SPIRV_BINARY_DIR} ${SPIRV_FILE})
    get_filename_component(SPIRV_INSTALL_DIR "${SHADERS_INSTALL_DIR}/${REL_PATH}" DIRECTORY)
    # Install
    install(FILES ${SPIRV_FILE} DESTINATION ${SPIRV_INSTALL_DIR})
endforeach()