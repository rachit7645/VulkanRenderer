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
set(SHADERS_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/Bin/Assets/Shaders)

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

# Debug Without Validation config
if (CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
    list(APPEND SHADER_FLAGS -gVS)
endif()

# Includes
set(SHADER_INCLUDES -I${SHADER_SOURCE_DIR}/Include)

# Shader files
file(GLOB SHADER_SOURCES
    PostProcess.vert
    PostProcess.frag
    Forward.vert
    Forward.frag
    Depth.vert
    Depth.frag
    ImGui.vert
    ImGui.frag
    BRDF.frag
    BRDF.vert
    Converter.vert
    Converter.frag
    Skybox.frag
    Skybox.vert
    Convolution.frag
    Convolution.vert
    PreFilter.vert
    PreFilter.frag
    DownSample.vert
    DownSample.frag
    UpSample.frag
    UpSample.vert
    Shadow.frag
    Shadow.vert
)

# Shader headers
file(GLOB_RECURSE SHADER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/*.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/Constants/*.glsl
)

# Ensure CMake reconfigures when new includes are added
set_property(GLOBAL PROPERTY CMAKE_CONFIGURE_DEPENDS ${SHADER_HEADERS})

# For each shader to be compiled
foreach(Shader ${SHADER_SOURCES})
    # Get file name
    get_filename_component(FILE_NAME ${Shader} NAME)
    # Set resulting spir-v file name
    set(SPIRV "${SPIRV_BINARY_DIR}/${FILE_NAME}.spv")
    # Compile shader
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_BINARY_DIR}"
        COMMAND ${GLSL_COMPILER} ${Shader} ${SHADER_FLAGS} ${SHADER_INCLUDES} -o ${SPIRV}
        DEPENDS ${Shader} ${SHADER_HEADERS}
        COMMENT "Building GLSL shader ${Shader}"
    )
    # Add compiled shader to list
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(Shader)

# Create custom shaders target
add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})

# Install
install(FILES ${SPIRV_BINARY_FILES} DESTINATION ${SHADERS_INSTALL_DIR})