# MIT License

# Copyright (c) 2025 Mateusz Gancarz

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 23)
project(chaos-in-multiplayer LANGUAGES C CXX)

add_subdirectory(ThirdParty)

file(GLOB_RECURSE CHAOS_IN_MULTIPLAYER_COMMON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Source/Common/*.cpp)
add_library(chaos-in-multiplayer-common STATIC ${CHAOS_IN_MULTIPLAYER_COMMON_SOURCES})
target_include_directories(chaos-in-multiplayer-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Include)
target_link_libraries(chaos-in-multiplayer-common
    GameNetworkingSockets::shared
    spdlog::spdlog)

file(GLOB_RECURSE CHAOS_IN_MULTIPLAYER_SERVER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Source/Server/*.cpp)
add_library(chaos-in-multiplayer-server STATIC ${CHAOS_IN_MULTIPLAYER_SERVER_SOURCES})
target_include_directories(chaos-in-multiplayer-server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Include)
target_link_libraries(chaos-in-multiplayer-server chaos-in-multiplayer-common)

file(GLOB_RECURSE CHAOS_IN_MULTIPLAYER_CLIENT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/Source/Client/*.cpp)
add_library(chaos-in-multiplayer-client STATIC ${CHAOS_IN_MULTIPLAYER_CLIENT_SOURCES})
target_include_directories(chaos-in-multiplayer-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Include)
target_link_libraries(chaos-in-multiplayer-client chaos-in-multiplayer-common)

add_subdirectory(Examples)
