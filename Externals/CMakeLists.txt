cmake_minimum_required(VERSION 3.25)
project(Externals)

# C++ 20 is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Required packages
find_package(SDL2   REQUIRED)
find_package(Vulkan REQUIRED)

# ImGui source files
add_library(imgui SHARED
	ImGui/imgui.cpp
	ImGui/imgui_draw.cpp
	ImGui/imgui_tables.cpp
	ImGui/imgui_widgets.cpp
	# ImGui/imgui_demo.cpp
	ImGui/backends/imgui_impl_sdl2.cpp
	ImGui/backends/imgui_impl_vulkan.cpp
	ImGui/misc/cpp/imgui_stdlib.cpp
)

# ImGui precompiled headers
target_precompile_headers(imgui PRIVATE
	ImGui/imconfig.h
	ImGui/imgui_internal.h
	ImGui/imgui.h
	ImGui/imstb_rectpack.h
	ImGui/imstb_truetype.h
	ImGui/backends/imgui_impl_sdl2.h
	ImGui/backends/imgui_impl_vulkan.h
	ImGui/misc/cpp/imgui_stdlib.h
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