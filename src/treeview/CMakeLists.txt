# wubupad tree view + git status
add_library(wubupad_treeview STATIC treeview.c)
target_include_directories(wubupad_treeview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_treeview test_treeview.c)
target_link_libraries(test_treeview PRIVATE wubupad_treeview)
add_test(NAME treeview COMMAND test_treeview)
