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

# Acceptance test for the core.
add_executable(test_core tests/test_core.c)
target_link_libraries(test_core PRIVATE wubupad_core)
add_test(NAME core COMMAND test_core)

# Unit test for lexer fold regions + symbol (function-list) extraction.
add_executable(test_lex_fold tests/test_lex_fold.c)
target_link_libraries(test_lex_fold PRIVATE wubupad_core)
add_test(NAME lex_fold COMMAND test_lex_fold)

# UI abstraction layer (opaque view/renderer over the core).
# Split into a headless-only UI lib (no windowing deps, reused by the agent,
# tests, and the WuBuOffice bridge) and an optional gfx lib (SDL2+FreeType2)
# that only adds the graphical backend. This keeps SDL2/Freetype out of the
# headless path entirely.
find_package(SDL2 QUIET)
find_package(Freetype QUIET)
set(WUBUPAD_WITH_GFX OFF)
if(SDL2_FOUND AND FREETYPE_FOUND)
  set(WUBUPAD_WITH_GFX ON)
  add_compile_definitions(WUBUPAD_WITH_GFX=1)
  message(STATUS "WuBuPad: gfx backend ENABLED (SDL2 + FreeType2)")
else()
  message(STATUS "WuBuPad: gfx backend DISABLED (SDL2/FreeType2 not found)")
endif()

# Headless UI lib: ui.c + headless + tty backends (no SDL2/Freetype).
add_library(wubupad_ui STATIC
  src/ui/ui.c src/ui/ui_headless.c src/ui/ui_tty.c
  src/ui/ui_find.c src/ui/ui_theme.c src/ui/ui_macro.c src/ui/ui_atom.c)
target_include_directories(wubupad_ui PUBLIC src
  ${CMAKE_SOURCE_DIR}/src/command ${CMAKE_SOURCE_DIR}/src/fuzzy
  ${CMAKE_SOURCE_DIR}/src/palette ${CMAKE_SOURCE_DIR}/src/pkgmgr)
target_link_libraries(wubupad_ui PUBLIC wubupad_core wubupad_atom)

# --- Text shaping (HarfBuzz + FriBidi) -- optional, gates wubushape ---
find_package(PkgConfig QUIET)
set(WUBUPAD_WITH_SHAPE OFF)
if(PkgConfig_FOUND)
  pkg_check_modules(HARFBUZZ QUIET harfbuzz)
  pkg_check_modules(FRIBIDI  QUIET fribidi)
  if(HARFBUZZ_FOUND AND FRIBIDI_FOUND)
    set(WUBUPAD_WITH_SHAPE ON)
    add_compile_definitions(WUBUPAD_WITH_SHAPE=1)
    message(STATUS "WuBuPad: text shaping ENABLED (HarfBuzz + FriBidi)")
  else()
    message(STATUS "WuBuPad: text shaping DISABLED (HarfBuzz/FriBidi not found)")
  endif()
else()
  message(STATUS "WuBuPad: text shaping DISABLED (pkg-config unavailable)")
endif()

# Shaping lib (always defined; empty stub when deps missing).
add_library(wubushape STATIC src/shape/shape.c)
target_include_directories(wubushape PUBLIC src)
if(WUBUPAD_WITH_SHAPE)
  target_include_directories(wubushape PUBLIC ${HARFBUZZ_INCLUDE_DIRS} ${FRIBIDI_INCLUDE_DIRS})
  target_link_libraries(wubushape PUBLIC ${HARFBUZZ_LIBRARIES} ${FRIBIDI_LIBRARIES})
endif()

# Graphical backend (optional). Provides ui_gfx_backend() on top of wubupad_ui.
if(WUBUPAD_WITH_GFX)
  add_library(wubupad_gfx STATIC src/ui/ui_gfx.c)
  target_include_directories(wubupad_gfx PUBLIC src)
  target_link_libraries(wubupad_gfx PUBLIC wubupad_ui wubushape SDL2::SDL2 Freetype::Freetype)
endif()

# === Atom absorption: package-driven, hackable editor subsystem ===
# Each feature is an opaque module (command registry, fuzzy finder, palette,
# package manager, tree view, snippets, multi-cursor, auto-indent, minimap,
# markdown preview). They are wired into the UI controller in src/ui/ui.c.
add_subdirectory(src/command)
add_subdirectory(src/fuzzy)
add_subdirectory(src/palette)
add_subdirectory(src/pkgmgr)
add_subdirectory(src/treeview)
add_subdirectory(src/snippet)
add_subdirectory(src/multicursor)
add_subdirectory(src/autoindent)
add_subdirectory(src/minimap)
add_subdirectory(src/mdpreview)

# Aggregate lib so front-ends link the whole Atom layer in one go.
add_library(wubupad_atom STATIC
  src/command/command.c src/fuzzy/fuzzy.c src/palette/palette.c
  src/pkgmgr/pkgmgr.c src/treeview/treeview.c src/snippet/snippet.c
  src/multicursor/multicursor.c src/autoindent/autoindent.c
  src/minimap/minimap.c src/mdpreview/mdpreview.c)
target_include_directories(wubupad_atom PUBLIC
  src/command src/fuzzy src/palette src/pkgmgr src/treeview
  src/snippet src/multicursor src/autoindent src/minimap src/mdpreview)
target_link_libraries(wubupad_atom PUBLIC
  wubupad_core wubupad_command wubupad_fuzzy wubupad_palette
  wubupad_pkgmgr wubupad_treeview wubupad_snippet wubupad_multicursor
  wubupad_autoindent wubupad_minimap wubupad_mdpreview)

# wubuOS-facing CLI: NDJSON in -> NDJSON out, plus an interactive TUI/gfx mode.
add_executable(wubupad apps/wubupad/main.c apps/wubupad/smoke.c)
if(WUBUPAD_WITH_GFX)
  target_link_libraries(wubupad PRIVATE wubupad_agent wubupad_ui wubupad_gfx wubupad_atom SDL2::SDL2 Freetype::Freetype)
else()
  target_link_libraries(wubupad PRIVATE wubupad_agent wubupad_ui wubupad_atom)
endif()

# Headless screenshot tool (renders one frame to a raw RGBA file).
if(SDL2_FOUND AND FREETYPE_FOUND)
  add_executable(shot apps/wubupad/shot.c)
  target_link_libraries(shot PRIVATE wubupad_ui wubupad_gfx wubupad_core SDL2::SDL2 Freetype::Freetype)
endif()

# Acceptance test for the Atom subsystem (command palette driving the UI).
add_executable(test_atom tests/test_atom.c)
target_link_libraries(test_atom PRIVATE wubupad_ui)
add_test(NAME atom COMMAND test_atom)

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

# GUI human-parity: capture a WuBuPad frame and pixel-audit chrome regions
# (menu, tabs, status). Fails if any is illegible. No vision, fast dev-cycle.
add_test(NAME gui_parity COMMAND bash ${CMAKE_SOURCE_DIR}/../tooling/gui_audit_pad.sh)
set_tests_properties(gui_parity PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)

# WCAG AA palette gate: every text token vs its surface must be >= 4.5:1
# (dark + light). Deterministic, no image. Catches dim-text accessibility
# regressions (research 2026-08-12: dark mode must test each color, not assume).
add_test(NAME wcag COMMAND python3 ${CMAKE_SOURCE_DIR}/../tooling/wcag_palette.py)
set_tests_properties(wcag PROPERTIES LABELS "accessibility;gui")

# Graphics backend test (only meaningful when gfx is linked; test guards itself
# by returning PASS-skip if SDL/FreeType were unavailable at build time).
if(SDL2_FOUND AND FREETYPE_FOUND)
  add_executable(test_ui_gfx tests/test_ui_gfx.c)
  target_link_libraries(test_ui_gfx PRIVATE wubupad_gfx SDL2::SDL2 Freetype::Freetype)
  add_test(NAME ui_gfx COMMAND test_ui_gfx)
endif()

# Acceptance test for text shaping (HarfBuzz + FriBidi).
if(WUBUPAD_WITH_SHAPE)
  add_executable(test_shape tests/test_shape.c)
  target_link_libraries(test_shape PRIVATE wubushape Freetype::Freetype)
  add_test(NAME shape COMMAND test_shape)
endif()

# Acceptance test for the agent protocol.
add_executable(test_agent tests/test_agent.c)
target_link_libraries(test_agent PRIVATE wubupad_agent)
add_test(NAME agent COMMAND test_agent)
