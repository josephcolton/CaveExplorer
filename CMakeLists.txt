cmake_minimum_required(VERSION 3.25)
project(CaveExplorer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

include(FetchContent)

# --- Dependencies -----------------------------------------------------------
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.8
  GIT_SHALLOW TRUE)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  SYSTEM)

# SDL_image: PNG/BMP only, using its built-in stb_image backend (no extra system libs).
set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_image
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
  GIT_TAG release-3.2.6
  GIT_SHALLOW TRUE)

# SDL_mixer: WAV, OGG (stb_vorbis), MP3 (dr_mp3), FLAC (dr_flac) only - all built in, no system libs.
set(SDLMIXER_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC_LIBFLAC OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3_mixer
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
  GIT_TAG release-3.2.4
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(SDL3 json SDL3_image SDL3_mixer)

# --- Game logic without SDL (shared with the unit tests) ---------------------
add_library(cave_logic STATIC
  src/game/Items.cpp
  src/game/Inventory.cpp
  src/game/WorldSave.cpp
  src/game/Character.cpp
  src/game/SaveGame.cpp
  src/game/Modifiers.cpp
  src/game/Quests.cpp)
target_include_directories(cave_logic PUBLIC src)
target_link_libraries(cave_logic PUBLIC nlohmann_json::nlohmann_json)

# --- Game -------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM GAME_SOURCES
  ${CMAKE_SOURCE_DIR}/src/game/Items.cpp
  ${CMAKE_SOURCE_DIR}/src/game/Inventory.cpp
  ${CMAKE_SOURCE_DIR}/src/game/WorldSave.cpp
  ${CMAKE_SOURCE_DIR}/src/game/Character.cpp
  ${CMAKE_SOURCE_DIR}/src/game/SaveGame.cpp
  ${CMAKE_SOURCE_DIR}/src/game/Modifiers.cpp
  ${CMAKE_SOURCE_DIR}/src/game/Quests.cpp)
add_executable(cave_explorer ${GAME_SOURCES})
target_link_libraries(cave_explorer PRIVATE cave_logic SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer)

if(MSVC)
  target_compile_options(cave_explorer PRIVATE /W4 /permissive-)
else()
  target_compile_options(cave_explorer PRIVATE -Wall -Wextra)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # False positives from nlohmann/json once inlined at -O2.
  target_compile_options(cave_explorer PRIVATE -Wno-maybe-uninitialized)
endif()

if(MINGW)
  # Avoid shipping libstdc++/libgcc DLLs alongside the game.
  target_link_options(cave_explorer PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Copy runtime DLLs (SDL3.dll) and game content next to the executable.
if(WIN32)
  add_custom_command(TARGET cave_explorer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:cave_explorer> $<TARGET_FILE_DIR:cave_explorer>
    COMMAND_EXPAND_LISTS)
endif()
# Runs on every build so edited maps/data are always picked up.
add_custom_target(copy_content ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/maps $<TARGET_FILE_DIR:cave_explorer>/assets/maps
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/tiles $<TARGET_FILE_DIR:cave_explorer>/assets/tiles
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/sprites $<TARGET_FILE_DIR:cave_explorer>/assets/sprites
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/soundtrack $<TARGET_FILE_DIR:cave_explorer>/assets/soundtrack
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/sounds $<TARGET_FILE_DIR:cave_explorer>/assets/sounds
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:cave_explorer>/data
  COMMENT "Copying game content")
add_dependencies(cave_explorer copy_content)

# --- Unit tests ---------------------------------------------------------------
option(CAVE_BUILD_TESTS "Build unit tests" ON)
if(CAVE_BUILD_TESTS)
  FetchContent_Declare(doctest
    URL https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
    DOWNLOAD_NO_EXTRACT TRUE)
  FetchContent_MakeAvailable(doctest)
  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
  add_executable(cave_tests ${TEST_SOURCES})
  target_include_directories(cave_tests PRIVATE ${doctest_SOURCE_DIR})
  target_link_libraries(cave_tests PRIVATE cave_logic)
  if(MINGW)
    target_link_options(cave_tests PRIVATE -static-libgcc -static-libstdc++ -static)
  endif()
  enable_testing()
  add_test(NAME cave_tests COMMAND cave_tests)
endif()
