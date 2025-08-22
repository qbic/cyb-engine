configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h @ONLY)

file(GLOB_RECURSE SOURCE_FILES *.cpp *.c *.h)

# Shader files will only be added to the source groups for easy access when
# using an IDE and is not part of the library source files
file(GLOB_RECURSE SHADER_FILES *.frag *.vert *.geom *.glsl)

# Let cmake create source groups based on file paths, to make the project
# easier to navigate though an IDE
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})

add_library(cyb-engine STATIC ${SOURCE_FILES})
target_link_libraries(cyb-engine PRIVATE ${Vulkan_LIBRARIES} shaderc_shared spirv-cross-reflect imgui)

target_compile_options(cyb-engine PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/MP /Zi /JMC /Ox /Oi /Os /Oy /GT /GF- /GS- /Gy- /fp:fast>)
target_compile_options(cyb-engine PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/MP>)

target_compile_definitions(cyb-engine PRIVATE $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>)

target_link_directories(cyb-engine PUBLIC
  $ENV{VULKAN_SDK}/Lib
  ${imgui_BINARY_DIR}
)

target_include_directories(cyb-engine PUBLIC
  ${CMAKE_BINARY_DIR}
  ${imgui_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/engine
  ${PROJECT_SOURCE_DIR}/engine/third_party
  ${Vulkan_INCLUDE_DIR}
)
