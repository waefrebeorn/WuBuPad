# wubupad multiple cursors
add_library(wubupad_multicursor STATIC multicursor.c)
target_include_directories(wubupad_multicursor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_multicursor test_multicursor.c)
target_link_libraries(test_multicursor PRIVATE wubupad_multicursor)
add_test(NAME multicursor COMMAND test_multicursor)
