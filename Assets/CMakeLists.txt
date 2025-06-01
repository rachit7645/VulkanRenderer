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
project(Assets)

# Install location
set(ASSETS_SOURCE_DIR  ${CMAKE_SOURCE_DIR}/Assets)
set(ASSETS_INSTALL_DIR ${CMAKE_SOURCE_DIR}/Bin/Assets)

# Shaders
add_subdirectory(Shaders)

# Copy Assets
install(DIRECTORY ${ASSETS_SOURCE_DIR}/GFX    DESTINATION ${ASSETS_INSTALL_DIR})
install(DIRECTORY ${ASSETS_SOURCE_DIR}/Scenes DESTINATION ${ASSETS_INSTALL_DIR})

# Copy misc files
install(FILES ${CMAKE_SOURCE_DIR}/Assets/Config.json DESTINATION ${ENGINE_INSTALL_DIR}/Assets)