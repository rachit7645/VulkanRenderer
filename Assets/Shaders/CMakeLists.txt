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

# Find Python
find_package(Vulkan  REQUIRED COMPONENTS glslangValidator)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Shader locations
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SPIRV_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR})

# Compilation script
set(SHADER_COMPILE_SCRIPT ${SHADER_SOURCE_DIR}/CompileShader.py)

# Shader files
file(GLOB_RECURSE SHADER_SOURCES
    ${SHADER_SOURCE_DIR}/*.vert
    ${SHADER_SOURCE_DIR}/*.frag
    ${SHADER_SOURCE_DIR}/*.comp
    ${SHADER_SOURCE_DIR}/*.rgen
    ${SHADER_SOURCE_DIR}/*.rmiss
    ${SHADER_SOURCE_DIR}/*.rahit
)

# Shader headers
file(GLOB_RECURSE SHADER_HEADERS
    ${SHADER_SOURCE_DIR}/Include/*.glsl
    ${CMAKE_SOURCE_DIR}/Shared/*.h
)

# Ensure CMake reconfigures
set_property(GLOBAL PROPERTY CMAKE_CONFIGURE_DEPENDS ${SHADER_HEADERS} ${SHADER_SOURCES})

# Determine compilation arguments
set(SHADER_COMPILE_ARGS)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "Profile")
    list(APPEND SHADER_COMPILE_ARGS --release)
endif()

# Generate commands
set(SPIRV_BINARY_FILES)
foreach(Shader ${SHADER_SOURCES})
    # Get relative path
    file(RELATIVE_PATH REL_PATH ${SHADER_SOURCE_DIR} ${Shader})
    # Create output path
    set(SPIRV_BINARY ${SHADERS_INSTALL_DIR}/${REL_PATH}.spv)
    # Append to list
    list(APPEND SPIRV_BINARY_FILES ${SPIRV_BINARY})
    # Build Command
    add_custom_command(
        OUTPUT ${SPIRV_BINARY}
        COMMAND ${Python3_EXECUTABLE} ${SHADER_COMPILE_SCRIPT} ${Shader} ${SHADER_COMPILE_ARGS}
        DEPENDS ${SHADER_COMPILE_SCRIPT} ${Shader} ${SHADER_HEADERS}
        COMMENT "Compiling Shader ${REL_PATH}"
    )
endforeach()

# Shaders Target
add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})