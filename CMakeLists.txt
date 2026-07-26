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
  src/json.c
  src/complete.c)
target_include_directories(wubupad_core PUBLIC src)

# Agent protocol library (the machine-facing "AGI GUI" over the core).
add_library(wubupad_agent STATIC src/agent.c)
target_link_libraries(wubupad_agent PUBLIC wubupad_core)
target_include_directories(wubupad_agent PUBLIC src)

# wubuOS-facing CLI: NDJSON in -> NDJSON out (document ingestion engine),
# plus an interactive TUI mode (--edit) backed by the UI layer.
add_executable(wubupad apps/wubupad/main.c)
target_link_libraries(wubupad PRIVATE wubupad_agent wubupad_ui)

# Acceptance test for the core.
add_executable(test_core tests/test_core.c)
target_link_libraries(test_core PRIVATE wubupad_core)
add_test(NAME core COMMAND test_core)

# UI abstraction layer (opaque view/renderer over the core).
# The gfx backend (SDL2 + FreeType2) is optional; if both libs are found we
# compile ui_gfx.c and link them into the editor app. Without them the TUI
# backend still builds (no hard dep).
find_package(SDL2 QUIET)
find_package(Freetype QUIET)
set(UI_SOURCES src/ui/ui.c src/ui/ui_headless.c src/ui/ui_tty.c)
set(UI_LIBS wubupad_core)
if(SDL2_FOUND AND FREETYPE_FOUND)
  list(APPEND UI_SOURCES src/ui/ui_gfx.c)
  list(APPEND UI_LIBS SDL2::SDL2 Freetype::Freetype)
  add_compile_definitions(WUBUPAD_WITH_GFX=1)
  message(STATUS "WuBuPad: gfx backend ENABLED (SDL2 + FreeType2)")
else()
  message(STATUS "WuBuPad: gfx backend DISABLED (SDL2/FreeType2 not found)")
endif()

add_library(wubupad_ui STATIC ${UI_SOURCES} src/ui/ui_find.c src/ui/ui_theme.c src/ui/ui_macro.c)
target_include_directories(wubupad_ui PUBLIC src)
target_link_libraries(wubupad_ui PUBLIC ${UI_LIBS})

# Editor app links the gfx libs too (main.c references ui_gfx_backend).
if(SDL2_FOUND AND FREETYPE_FOUND)
  target_link_libraries(wubupad PRIVATE wubupad_agent wubupad_ui SDL2::SDL2 Freetype::Freetype)
endif()

# Acceptance test for folding (Phase D).
add_executable(test_fold tests/test_fold.c)
target_link_libraries(test_fold PRIVATE wubupad_ui)
add_test(NAME fold COMMAND test_fold)

# Acceptance test for completion + symbols (Phase D).
add_executable(test_complete tests/test_complete.c)
target_link_libraries(test_complete PRIVATE wubupad_ui)
add_test(NAME complete COMMAND test_complete)

# Acceptance test for editor features (Phase D).
add_executable(test_editor tests/test_editor.c)
target_link_libraries(test_editor PRIVATE wubupad_ui)
add_test(NAME editor COMMAND test_editor)

# Acceptance test for theme/status/tabs (Phase C).
add_executable(test_theme tests/test_theme.c)
target_link_libraries(test_theme PRIVATE wubupad_ui)
add_test(NAME theme COMMAND test_theme)

# Acceptance test for find/replace (Phase B).
add_executable(test_find tests/test_find.c)
target_link_libraries(test_find PRIVATE wubupad_ui)
add_test(NAME find COMMAND test_find)

# Acceptance test for the UI controller (driven on the headless backend).
add_executable(test_ui tests/test_ui.c)
target_link_libraries(test_ui PRIVATE wubupad_ui)
add_test(NAME ui COMMAND test_ui)

# Graphics backend test (only meaningful when gfx is linked; test guards itself
# by returning PASS-skip if SDL/FreeType were unavailable at build time).
if(SDL2_FOUND AND FREETYPE_FOUND)
  add_executable(test_ui_gfx tests/test_ui_gfx.c)
  target_link_libraries(test_ui_gfx PRIVATE wubupad_ui SDL2::SDL2 Freetype::Freetype)
  add_test(NAME ui_gfx COMMAND test_ui_gfx)
endif()

# Acceptance test for the agent protocol.
add_executable(test_agent tests/test_agent.c)
target_link_libraries(test_agent PRIVATE wubupad_agent)
add_test(NAME agent COMMAND test_agent)
