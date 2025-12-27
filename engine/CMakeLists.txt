configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h @ONLY)

#--- Library
file(GLOB_RECURSE SOURCE_FILES *.cpp *.c *.h)

# Shader files will only be added to the source groups for easy access when
# using an IDE and is not part of the library source files
file(GLOB_RECURSE SHADER_FILES *.frag *.vert *.geom *.glsl)
list(APPEND SOURCE_FILES ${SHADER_FILES})

# Let cmake create source groups based on file paths, to make the project
# easier to navigate through an IDE
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})

add_library(cyb-engine STATIC ${SOURCE_FILES})
target_link_libraries(cyb-engine PRIVATE ${Vulkan_LIBRARIES} shaderc_shared spirv-cross-reflect imgui)

#--- Compiler switches
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ARCH_SSE2 $<$<CXX_COMPILER_ID:MSVC>:/arch:SSE2> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-msse2>)
else()
    set(ARCH_SSE2 $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-msse2>)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    list(APPEND ARCH_SSE2 -mfpmath=sse)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE ${ARCH_SSE2})

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE /MP /sdl /Zc:inline /fp:fast)

    if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        target_compile_options(${PROJECT_NAME} PRIVATE $<$<NOT:$<CONFIG:DEBUG>>:/Gy /Gw>)
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.10)
        target_compile_options(${PROJECT_NAME} PRIVATE /permissive-)
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.14)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus)
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.24)
        target_compile_options(${PROJECT_NAME} PRIVATE /ZH:SHA_256)
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /wd5105)
    endif()
endif()

#target_compile_definitions(cyb-engine PRIVATE $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>)
target_link_directories(cyb-engine PUBLIC $ENV{VULKAN_SDK}/Lib ${imgui_BINARY_DIR})

target_include_directories(cyb-engine PUBLIC
  ${CMAKE_BINARY_DIR}
  ${imgui_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/engine
  ${PROJECT_SOURCE_DIR}/engine/third_party
  ${Vulkan_INCLUDE_DIR})
