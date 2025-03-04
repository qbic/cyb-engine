
file(GLOB SOURCE_FILES *.cpp *.h)
list (APPEND SOURCE_FILES game.rc)

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})

add_executable(game WIN32 ${SOURCE_FILES})
target_link_libraries(game PUBLIC cyb-engine)

