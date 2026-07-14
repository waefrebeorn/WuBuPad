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

# Headless editing core (reused by GUI backends, the agent, and tests).
add_library(wubupad_core STATIC
  src/buffer.c
  src/lex.c
  src/doc.c
  src/search.c
  src/encode.c
  src/diff.c
  src/docs.c
  src/json.c)
target_include_directories(wubupad_core PUBLIC src)

# Agent protocol library (the machine-facing "AGI GUI" over the core).
add_library(wubupad_agent STATIC src/agent.c)
target_link_libraries(wubupad_agent PUBLIC wubupad_core)
target_include_directories(wubupad_agent PUBLIC src)

# wubuOS-facing CLI: NDJSON in -> NDJSON out (document ingestion engine).
add_executable(wubupad apps/wubupad/main.c)
target_link_libraries(wubupad PRIVATE wubupad_agent)

# Acceptance test for the core.
add_executable(test_core tests/test_core.c)
target_link_libraries(test_core PRIVATE wubupad_core)
add_test(NAME core COMMAND test_core)

# Acceptance test for the agent protocol.
add_executable(test_agent tests/test_agent.c)
target_link_libraries(test_agent PRIVATE wubupad_agent)
add_test(NAME agent COMMAND test_agent)
