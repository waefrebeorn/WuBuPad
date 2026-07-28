# wubupad auto-indent / smart typing
add_library(wubupad_autoindent STATIC autoindent.c)
target_include_directories(wubupad_autoindent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_autoindent test_autoindent.c)
target_link_libraries(test_autoindent PRIVATE wubupad_autoindent)
add_test(NAME autoindent COMMAND test_autoindent)
