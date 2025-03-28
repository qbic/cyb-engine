configure_file(config.h.in ${CMAKE_BINARY_DIR}/config.h @ONLY)

file(GLOB_RECURSE SOURCE_FILES *.cpp *.c *.h)

# Shader files will only be added to the source groups for easy access when
# using an IDE and is not part of the library source files
file(GLOB_RECURSE SHADER_FILES *.frag *.vert *.geom *.glsl)

# Remove imgui's built-in backends sence we use our own one
list(FILTER SOURCE_FILES EXCLUDE REGEX "/third_party/imgui/backends/*")

# Remove all freetype sources from SOURCE_LIST and create a new list for all the
# freetpye sources, this way we can set some custom compiler flags fot theese files only
# then push them back to the SOURCE_FILES list
list(FILTER SOURCE_FILES EXCLUDE REGEX "/third_party/freetype/*")
set(FT_FILES 
  third_party/freetype/src/autofit/autofit.c
  third_party/freetype/src/base/ftbase.c
  third_party/freetype/src/base/ftbbox.c
  third_party/freetype/src/base/ftbdf.c
  third_party/freetype/src/base/ftbitmap.c
  third_party/freetype/src/base/ftcid.c
  third_party/freetype/src/base/ftfstype.c
  third_party/freetype/src/base/ftgasp.c
  third_party/freetype/src/base/ftglyph.c
  third_party/freetype/src/base/ftgxval.c
  third_party/freetype/src/base/ftinit.c
  third_party/freetype/src/base/ftmm.c
  third_party/freetype/src/base/ftotval.c
  third_party/freetype/src/base/ftpatent.c
  third_party/freetype/src/base/ftpfr.c
  third_party/freetype/src/base/ftstroke.c
  third_party/freetype/src/base/ftsynth.c
  third_party/freetype/src/base/fttype1.c
  third_party/freetype/src/base/ftwinfnt.c
  third_party/freetype/src/bdf/bdf.c
  third_party/freetype/src/bzip2/ftbzip2.c
  third_party/freetype/src/cache/ftcache.c
  third_party/freetype/src/cff/cff.c
  third_party/freetype/src/cid/type1cid.c
  third_party/freetype/src/gzip/ftgzip.c
  third_party/freetype/src/lzw/ftlzw.c
  third_party/freetype/src/pcf/pcf.c
  third_party/freetype/src/pfr/pfr.c
  third_party/freetype/src/psaux/psaux.c
  third_party/freetype/src/pshinter/pshinter.c
  third_party/freetype/src/psnames/psnames.c
  third_party/freetype/src/raster/raster.c
  third_party/freetype/src/sdf/sdf.c
  third_party/freetype/src/sfnt/sfnt.c
  third_party/freetype/src/smooth/smooth.c
  third_party/freetype/src/svg/svg.c
  third_party/freetype/src/truetype/truetype.c
  third_party/freetype/src/type1/type1.c
  third_party/freetype/src/type42/type42.c
  third_party/freetype/src/winfonts/winfnt.c
)
set_property(SOURCE ${FT_FILES} PROPERTY COMPILE_DEFINITIONS FT2_BUILD_LIBRARY VC_EXTRALEAN WIN32 WIN32_LEAN_AND_MEAN)
list(APPEND SOURCE_FILES ${FT_FILES} ${SHADER_FILES})

# Let cmake create source groups based on file paths, to make the project
# easier to navigate though an IDE
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})

add_library(cyb-engine STATIC ${SOURCE_FILES})
target_link_libraries(cyb-engine ${Vulkan_LIBRARIES} shaderc_shared spirv-cross-reflect)

target_compile_options(cyb-engine PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/MP /Zi /JMC /Ox /Oi /Os /Oy /GT /GF- /GS- /Gy- /fp:fast>)
target_compile_options(cyb-engine PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/MP>)

target_compile_definitions(cyb-engine PRIVATE $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>)

target_link_directories(cyb-engine PUBLIC "$ENV{VULKAN_SDK}/Lib")

target_include_directories(cyb-engine PUBLIC
  "${CMAKE_BINARY_DIR}"
  "${PROJECT_SOURCE_DIR}/engine"
  "${PROJECT_SOURCE_DIR}/engine/third_party"
  "${PROJECT_SOURCE_DIR}/engine/third_party/imgui"
  "${PROJECT_SOURCE_DIR}/engine/third_party/freetype/include"
  "${Vulkan_INCLUDE_DIR}")
