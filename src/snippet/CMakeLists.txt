# wubupad snippets engine
add_library(wubupad_snippet STATIC snippet.c)
target_include_directories(wubupad_snippet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(test_snippet test_snippet.c)
target_link_libraries(test_snippet PRIVATE wubupad_snippet)
add_test(NAME snippet COMMAND test_snippet)
