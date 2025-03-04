cmake_minimum_required(VERSION 3.27)
project(cyb-engine)

set(CYB_VERSION "2025.01.0")

string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" _match "${CYB_VERSION}")
set(CYB_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(CYB_VERSION_MINOR "${CMAKE_MATCH_2}")
set(CYB_VERSION_PATCH "${CMAKE_MATCH_3}")

# Configure CMake global variables
set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find required packages
find_package(Vulkan REQUIRED)

add_definitions(-DUNICODE -D_UNICODE)
add_compile_definitions($<$<PLATFORM_ID:Windows>:NOMINMAX>)

add_subdirectory(engine)
add_subdirectory(game)

# Set default startup project for visual studio
set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT game)
set_target_properties(game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

message(STATUS "")
message(STATUS "Configured cyb-engine version ${CYB_VERSION} with the following options:")
message(STATUS "")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Vulkan: v${Vulkan_VERSION}")
message(STATUS "")