cmake_minimum_required(VERSION 3.27)

#--- Versioning
include(cmake/version.cmake)
project(cyb-engine
    VERSION ${CYB_VERSION}
    DESCRIPTION "A modern C++ game engine focused on high performance and ease of use."
    LANGUAGES CXX)

# Configure CMake global variables
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_CXX_STANDARD 23)
set(FETCHCONTENT_QUIET OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#--- Find required packages
find_package(Vulkan REQUIRED)
include(fetch-freetype)
include(fetch-imgui)

 # Setup global compile flags
add_definitions(-DUNICODE -D_UNICODE)
add_compile_definitions($<$<PLATFORM_ID:Windows>:NOMINMAX>)
add_compile_definitions($<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>)

# Add subdirs
add_subdirectory(engine)
add_subdirectory(game)

# Set default startup project for visual studio
set_property(DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT game)
set_target_properties(game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Vulkan: v${Vulkan_VERSION}")
message(STATUS "Version: ${CYB_VERSION} (build ${CYB_VERSION_BUILD}, git ${CYB_VERSION_HASH})")
message(STATUS "")

#--- Packaging
file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}/Bin/shaderc_shared.dll" SHADERC_DLL_PATH)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 OLD)
endif()
install(TARGETS game RUNTIME DESTINATION .)
install(FILES ${SHADERC_DLL_PATH} DESTINATION .)
install(FILES ${CMAKE_BINARY_DIR}/imgui.ini DESTINATION .)
install(DIRECTORY assets/ DESTINATION assets)
install(DIRECTORY engine/shaders/ DESTINATION engine/shaders)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "cyb-engine")
set(CPACK_PACKAGE_VERSION ${CYB_VERSION})

include(CPack)
