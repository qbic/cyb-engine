cmake_minimum_required(VERSION 3.27)
project(cyb-engine)

set(CYB_VERSION "2025.05.01")
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" _match "${CYB_VERSION}")
set(CYB_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(CYB_VERSION_MINOR "${CMAKE_MATCH_2}")
set(CYB_VERSION_PATCH "${CMAKE_MATCH_3}")

# Configure CMake global variables
set(CMAKE_CXX_STANDARD 23)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Select X86 processor features to use
option(USE_SSE4_1 "Enable SSE4.1" ON)
option(USE_SSE4_2 "Enable SSE4.2" ON)
option(USE_AVX "Enable AVX" ON)
option(USE_AVX2 "Enable AVX2" ON)
option(USE_AVX512 "Enable AVX512" OFF)
option(USE_LZCNT "Enable LZCNT" ON)
option(USE_TZCNT "Enable TZCNT" ON)
option(USE_F16C "Enable F16C" ON)
option(USE_FMADD "Enable FMADD" ON)

# Find required packages
find_package(Vulkan REQUIRED)

# Fetch statically linked dependencies
set(FETCHCONTENT_QUIET OFF)
include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.90)
FetchContent_Declare(freetype GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git GIT_TAG VER-2-14-1)
FetchContent_MakeAvailable(imgui freetype)

add_library(imgui)
target_include_directories(imgui PUBLIC 
    ${imgui_SOURCE_DIR}
    ${ImNodeFlow_SOURCE_DIR}/include
)
target_link_libraries(imgui PUBLIC freetype)
target_sources(imgui PUBLIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
 )

 # Setup global compile flags
add_definitions(-DUNICODE -D_UNICODE)
add_compile_definitions($<$<PLATFORM_ID:Windows>:NOMINMAX>)

# Add subdirs
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


# install rules
file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}/Bin/shaderc_shared.dll" SHADERC_DLL_PATH)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 OLD)
endif()
install(TARGETS game RUNTIME DESTINATION .)
install(FILES ${SHADERC_DLL_PATH} DESTINATION .)
install(FILES build/imgui.ini DESTINATION .)
install(DIRECTORY assets/ DESTINATION assets)
install(DIRECTORY engine/shaders/ DESTINATION engine/shaders)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "cyb-engine")
set(CPACK_PACKAGE_VERSION ${CYB_VERSION})

include(CPack)
