cmake_minimum_required(VERSION 3.25)
project(libs)

# C++ 20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(SDL2   REQUIRED)
find_package(Vulkan REQUIRED)

# ImGui source files
add_library(imgui SHARED
	imgui/imgui.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_sdl2.cpp
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
	imgui/backends/imgui_impl_sdl2.h
	imgui/backends/imgui_impl_vulkan.h
	imgui/misc/cpp/imgui_stdlib.h
)

# SDL2 Includes
target_include_directories(imgui PRIVATE ${SDL2_INCLUDE_DIRS})
# ImGUI Includes
target_include_directories(imgui PUBLIC ImGui/ ImGui/backends ImGui/misc)
# ImGui Defines
target_compile_definitions(imgui PRIVATE IMGUI_DEFINE_MATH_OPERATORS=1)
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

# Link libraries
target_link_libraries(imgui Vulkan::Vulkan SDL2::SDL2)

# SIMD JSON source files
add_library(simdjson SHARED simdjson/singleheader/simdjson.cpp)
# SIMD JSON precompiled headers
target_precompile_headers(simdjson PRIVATE simdjson/singleheader/simdjson.h)

# SIMD JSON includes
target_include_directories(simdjson PUBLIC simdjson/singleheader/)
# Disable warnings
target_compile_options(simdjson PRIVATE -w)
# -O3 optimisations
target_compile_options(simdjson PRIVATE -O3)
# Enable CPU vectorisation (SSE or better)
target_compile_options(simdjson PRIVATE -march=native)

# Debug config
if (CMAKE_BUILD_TYPE STREQUAL Debug)
	# Enable debug output
	target_compile_options(simdjson PRIVATE -g)
endif()

# SIMD JSON libs
target_link_libraries(simdjson)