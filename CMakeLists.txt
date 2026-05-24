cmake_minimum_required(VERSION 4.0)
project(ptvc_framework)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_definitions("$<$<NOT:$<CONFIG:Debug>>:NDEBUG>")

set(JOLT_ENABLE_TESTS OFF CACHE BOOL "Disable Jolt unit tests" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "Disable Vulkan compute shaders" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "Disable Metal compute shaders" FORCE)

add_subdirectory(external/glm                   EXCLUDE_FROM_ALL)
add_subdirectory(external/sdl                   EXCLUDE_FROM_ALL)
add_subdirectory(external/spdlog                EXCLUDE_FROM_ALL)
add_subdirectory(external/VulkanMemoryAllocator EXCLUDE_FROM_ALL)
add_subdirectory(external/Jolt/Build            EXCLUDE_FROM_ALL)

# Engine library
add_subdirectory(source/engine)

# Game
add_subdirectory(source/game)
