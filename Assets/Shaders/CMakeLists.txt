cmake_minimum_required(VERSION 3.25)
project(Shaders)

# Install location
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(SHADERS_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/Bin/Assets/Shaders)

# Get glsl compiler
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_program(GLSL_COMPILER NAMES glslc HINTS Vulkan::glslc)

# Shader locations
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SPIRV_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR})

# Debug config
if (CMAKE_BUILD_TYPE STREQUAL Debug)
	# Debug shader flags
	set(SHADER_FLAGS -g -O)
else()
	# Release shader flags
	set(SHADER_FLAGS -O)
endif()

# Shader files
file(GLOB SHADER_SOURCES
    BasicShader.vert
    BasicShader.frag
)

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
        COMMAND ${GLSL_COMPILER} ${Shader} -o ${SPIRV} ${SHADER_FLAGS}
        DEPENDS ${Shader}
        COMMENT "Building GLSL shader ${Shader}"
    )
    # Add compiled shader to list
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(Shader)

# Create custom shaders target
add_custom_target(Shaders ALL DEPENDS ${SPIRV_BINARY_FILES})

# Install
install(FILES ${SPIRV_BINARY_FILES} DESTINATION ${SHADERS_INSTALL_DIR})