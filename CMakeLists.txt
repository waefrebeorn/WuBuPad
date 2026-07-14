cmake_minimum_required(VERSION 3.16)
project(wubupad C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(WITH_SANITIZER "Build with ASan + UBSan" OFF)

if(WITH_SANITIZER)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g)
  add_link_options(-fsanitize=address,undefined)
else()
  add_compile_options(-Wall -Wextra -Wshadow -O2 -g)
endif()

# Headless editing core (reused by GUI backends and tests).
add_library(wubupad_core STATIC
  src/buffer.c
  src/lex.c
  src/doc.c)
target_include_directories(wubupad_core PUBLIC src)

# Acceptance test for the core.
add_executable(test_core tests/test_core.c)
target_link_libraries(test_core PRIVATE wubupad_core)
add_test(NAME core COMMAND test_core)
