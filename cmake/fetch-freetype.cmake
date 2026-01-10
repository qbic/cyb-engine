# Fetch Freetype source from GitHub

include(FetchContent)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY  https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG         VER-2-14-1
    SOURCE_DIR      freetype)

FetchContent_MakeAvailable(freetype)