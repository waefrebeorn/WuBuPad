# wubupad markdown preview
add_library(wubupad_mdpreview STATIC mdpreview.c)
target_include_directories(wubupad_mdpreview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_mdpreview test_mdpreview.c)
target_link_libraries(test_mdpreview PRIVATE wubupad_mdpreview)
add_test(NAME mdpreview COMMAND test_mdpreview)
