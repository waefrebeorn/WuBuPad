# wubupad minimap
add_library(wubupad_minimap STATIC minimap.c)
target_include_directories(wubupad_minimap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_minimap test_minimap.c)
target_link_libraries(test_minimap PRIVATE wubupad_minimap)
add_test(NAME minimap COMMAND test_minimap)
