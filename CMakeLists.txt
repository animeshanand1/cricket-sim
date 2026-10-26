cmake_minimum_required(VERSION 3.25)
project(T20Coach VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(T20_BUILD_TESTS "Build the Catch2 test suite" ON)
option(T20_BUILD_APP "Build the desktop game (SDL3 + Dear ImGui)" ON)

# Bit-for-bit parity with the JS engine needs plain IEEE doubles: no FMA contraction, no fast-math.
add_library(t20_options INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(t20_options INTERFACE -ffp-contract=off -fno-fast-math -Wall -Wextra)
elseif(MSVC)
  target_compile_options(t20_options INTERFACE /fp:precise /utf-8 /W4 /permissive-)
endif()
if(MINGW)
  # One self-contained .exe: no libstdc++/libgcc DLLs needed next to it
  target_link_options(t20_options INTERFACE -static)
endif()

# --- core: JS number semantics, ordered map, JSON writer ---------------------
add_library(t20_core STATIC src/core/jsmath.cpp src/core/json.cpp)
target_include_directories(t20_core PUBLIC src)
target_link_libraries(t20_core PUBLIC t20_options)

# --- engine: pure game logic (no UI) ------------------------------------------
add_library(t20_engine STATIC
  src/engine/achievements.cpp
  src/engine/ageing.cpp
  src/engine/auction.cpp
  src/engine/board.cpp
  src/engine/career.cpp
  src/engine/club.cpp
  src/engine/development.cpp
  src/engine/editor.cpp
  src/engine/finance.cpp
  src/engine/fitness.cpp
  src/engine/league_view.cpp
  src/engine/milestones.cpp
  src/engine/popularity.cpp
  src/engine/records.cpp
  src/engine/season.cpp
  src/engine/sides.cpp
  src/engine/state_json.cpp
  src/engine/trades.cpp
  src/engine/world.cpp
  src/engine/ai.cpp
  src/engine/battle.cpp
  src/engine/commentary.cpp
  src/engine/fans.cpp
  src/engine/field.cpp
  src/engine/league.cpp
  src/engine/match.cpp
  src/engine/matchups.cpp
  src/engine/mindset.cpp
  src/engine/names.cpp
  src/engine/pitch.cpp
  src/engine/roster_data.cpp
  src/engine/rules.cpp
  src/engine/shots.cpp
  src/engine/skills.cpp
  src/engine/special.cpp
  src/engine/tactics.cpp
  src/engine/weather.cpp
  src/engine/winprob.cpp
)
target_link_libraries(t20_engine PUBLIC t20_core)

# --- app: the game state the UI drives (no UI code, so the tests can use it) ----
add_library(t20_app STATIC src/app/match_session.cpp src/app/game_state.cpp src/app/game.cpp src/app/save_manager.cpp)
target_link_libraries(t20_app PUBLIC t20_engine)

# --- parity: the C++ twin of tools/parity/dump_js.mjs -------------------------
add_library(t20_parity STATIC tools/parity/parity.cpp tools/parity/flow.cpp)
target_include_directories(t20_parity PUBLIC tools)
target_link_libraries(t20_parity PUBLIC t20_app)

# --- tools --------------------------------------------------------------------
add_executable(t20coach_balance tools/balance.cpp)
target_link_libraries(t20coach_balance PRIVATE t20_parity)

add_executable(t20coach_dump tools/dump.cpp)
target_link_libraries(t20coach_dump PRIVATE t20_parity)

# --- desktop game: SDL3 window, Dear ImGui, UI, app ------------------------------
if(T20_BUILD_APP)
  # SDL3 from source as a static library, so the exe needs no SDL3.dll
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/SDL3-3.2.10 EXCLUDE_FROM_ALL)

  set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-1.91.9b")
  add_library(t20_imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_include_directories(t20_imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
  target_link_libraries(t20_imgui PUBLIC SDL3::SDL3-static)

  # Fonts compiled into the exe
  include(cmake/embed_fonts.cmake)
  file(GLOB T20_FONT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/*.ttf")
  set(T20_FONTS_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/fonts_embedded.cpp")
  t20_embed_fonts("${T20_FONTS_CPP}" ${T20_FONT_FILES})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${T20_FONT_FILES})

  add_library(t20_ui STATIC
    src/ui/anim.cpp
    src/ui/app_shell.cpp
    src/ui/format.cpp
    src/ui/gfx.cpp
    src/ui/icons.cpp
    src/ui/theme.cpp
    src/ui/view_data.cpp
    src/ui/match/captain_panel.cpp
    src/ui/match/crease.cpp
    src/ui/match/field_view.cpp
    src/ui/match/impact_panel.cpp
    src/ui/match/moment_banner.cpp
    src/ui/match/scoreboard.cpp
    src/ui/match/win_and_fans.cpp
    src/ui/screens/club_screen.cpp
    src/ui/screens/common.cpp
    src/ui/screens/gallery_screen.cpp
    src/ui/screens/home_screen.cpp
    src/ui/screens/match_screen.cpp
    src/ui/screens/offseason_screen.cpp
    src/ui/screens/player_card.cpp
    src/ui/screens/players_screen.cpp
    src/ui/screens/screens.cpp
    src/ui/screens/season_screen.cpp
    src/ui/screens/toss_screen.cpp
    src/ui/screens/training_screen.cpp
    src/ui/screens/ui_context.cpp
    src/ui/screens/xi_screen.cpp
    src/ui/widgets/avatar.cpp
    src/ui/widgets/controls.cpp
    src/ui/widgets/forms.cpp
    src/ui/widgets/meter_and_rating.cpp
    src/ui/widgets/modal.cpp
    src/ui/widgets/nav_bar.cpp
    src/ui/widgets/panel.cpp
    src/ui/widgets/sortable_table.cpp
    src/ui/widgets/stepper.cpp
    src/ui/widgets/tags.cpp
    src/ui/widgets/team_badge.cpp
    ${T20_FONTS_CPP}
  )
  target_link_libraries(t20_ui PUBLIC t20_app t20_imgui)

  add_executable(t20coach WIN32 src/app/main.cpp)
  target_link_libraries(t20coach PRIVATE t20_ui)
endif()

# --- tests --------------------------------------------------------------------
if(T20_BUILD_TESTS)
  include(FetchContent)
  # A local copy (third_party/Catch2-3.8.1) is used when present, e.g. where CMake's own HTTPS
  # download has no CA bundle (WinLibs); otherwise it is downloaded.
  set(T20_CATCH2_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2-3.8.1")
  if(EXISTS "${T20_CATCH2_LOCAL}/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_CATCH2 "${T20_CATCH2_LOCAL}")
  endif()
  FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.8.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(Catch2)

  add_executable(t20_tests
    tests/jsmath_test.cpp
    tests/skills_test.cpp
    tests/ipl_test.cpp
    tests/engine_test.cpp
    tests/parity_test.cpp
    tests/session_test.cpp
    tests/flow_test.cpp
    tests/json_test.cpp
    tests/world_test.cpp
    tests/career_test.cpp
    tests/game_test.cpp
  )
  target_link_libraries(t20_tests PRIVATE t20_parity t20_app Catch2::Catch2WithMain)
  target_compile_definitions(t20_tests PRIVATE T20_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden")

  enable_testing()
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(t20_tests DISCOVERY_MODE PRE_TEST)
endif()
