cmake_minimum_required(VERSION 3.25)
project(CaveExplorer VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

option(CAVE_BUILD_TESTS "Build unit tests" ON)
option(CAVE_WINDOWS_CONSOLE "Windows: open a console window that shows log output" OFF)

include(FetchContent)

# --- Dependencies -----------------------------------------------------------
# Dependencies are built as part of this project; none of them install anything of their own.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.8
  GIT_SHALLOW TRUE)

set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  SYSTEM)

# SDL_image: PNG/BMP only, using its built-in stb_image backend (no extra system libs).
set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
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
set(SDLMIXER_INSTALL OFF CACHE BOOL "" FORCE)
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
set(LOGIC_SOURCES
  src/game/Items.cpp
  src/game/Inventory.cpp
  src/game/WorldSave.cpp
  src/game/Character.cpp
  src/game/SaveGame.cpp
  src/game/Modifiers.cpp
  src/game/Quests.cpp)
add_library(cave_logic STATIC ${LOGIC_SOURCES})
target_include_directories(cave_logic PUBLIC src)
target_link_libraries(cave_logic PUBLIC nlohmann_json::nlohmann_json)

# --- Game -------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
foreach(logic_source ${LOGIC_SOURCES})
  list(REMOVE_ITEM GAME_SOURCES ${CMAKE_SOURCE_DIR}/${logic_source})
endforeach()
add_executable(cave_explorer ${GAME_SOURCES})
target_link_libraries(cave_explorer PRIVATE cave_logic SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer)
target_compile_definitions(cave_explorer PRIVATE CAVE_VERSION="${PROJECT_VERSION}")

if(MSVC)
  target_compile_options(cave_explorer PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(cave_explorer PRIVATE -Wall -Wextra)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # False positives from nlohmann/json once inlined at -O2.
  target_compile_options(cave_explorer PRIVATE -Wno-maybe-uninitialized)
endif()

if(WIN32)
  # The .exe icon, and no console window unless asked for.
  target_sources(cave_explorer PRIVATE packaging/windows/cave_explorer.rc)
  if(NOT CAVE_WINDOWS_CONSOLE)
    set_target_properties(cave_explorer PROPERTIES WIN32_EXECUTABLE ON)
  endif()
endif()

if(MINGW)
  # Avoid shipping libstdc++/libgcc DLLs alongside the game.
  target_link_options(cave_explorer PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

if(UNIX AND NOT APPLE)
  # Installed/packaged builds find the bundled SDL libraries next to the executable.
  set_target_properties(cave_explorer PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Copy runtime DLLs (SDL3.dll) and game content next to the executable.
if(WIN32)
  add_custom_command(TARGET cave_explorer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:cave_explorer> $<TARGET_FILE_DIR:cave_explorer>
    COMMAND_EXPAND_LISTS)
endif()
set(CONTENT_DIRS assets/maps assets/tiles assets/sprites assets/soundtrack assets/sounds data)
set(COPY_COMMANDS)
foreach(dir ${CONTENT_DIRS})
  list(APPEND COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy_directory
       ${CMAKE_SOURCE_DIR}/${dir} $<TARGET_FILE_DIR:cave_explorer>/${dir})
endforeach()
# Runs on every build so edited maps/data are always picked up.
add_custom_target(copy_content ALL ${COPY_COMMANDS} COMMENT "Copying game content")
add_dependencies(cave_explorer copy_content)

# --- Install / package ---------------------------------------------------------
# `cmake --install build --prefix <dir>` lays the game out ready to run;
# `cpack` (from the build directory) zips that up as CaveExplorer-<version>-<platform>.
install(TARGETS cave_explorer RUNTIME DESTINATION .)
if(WIN32)
  install(FILES $<TARGET_RUNTIME_DLLS:cave_explorer> DESTINATION .)
else()
  foreach(lib SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer)
    install(FILES $<TARGET_SONAME_FILE:${lib}> DESTINATION . FOLLOW_SYMLINK_CHAIN)
  endforeach()
endif()
foreach(dir ${CONTENT_DIRS})
  get_filename_component(parent ${dir} DIRECTORY)
  install(DIRECTORY ${dir} DESTINATION ./${parent})
endforeach()
install(FILES README.md CREDITS.md LICENSE DESTINATION .)
if(UNIX AND NOT APPLE)
  install(FILES packaging/linux/cave-explorer.desktop assets/sprites/icon.png DESTINATION .)
endif()

if(WIN32)
  set(CAVE_PLATFORM windows-x64)
  set(CPACK_GENERATOR ZIP)
else()
  set(CAVE_PLATFORM linux-x64)
  set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_PACKAGE_NAME "CaveExplorer")
set(CPACK_PACKAGE_VENDOR "Cave Explorer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A 2D cave-exploring platformer")
set(CPACK_PACKAGE_FILE_NAME "CaveExplorer-${PROJECT_VERSION}-${CAVE_PLATFORM}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_STRIP_FILES ON)
include(CPack)

# --- Unit tests ---------------------------------------------------------------
if(CAVE_BUILD_TESTS)
  FetchContent_Declare(doctest
    URL https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
    DOWNLOAD_NO_EXTRACT TRUE)
  FetchContent_MakeAvailable(doctest)
  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
  add_executable(cave_tests ${TEST_SOURCES})
  target_include_directories(cave_tests PRIVATE ${doctest_SOURCE_DIR})
  target_link_libraries(cave_tests PRIVATE cave_logic)
  if(MSVC)
    target_compile_options(cave_tests PRIVATE /utf-8)
  endif()
  if(MINGW)
    target_link_options(cave_tests PRIVATE -static-libgcc -static-libstdc++ -static)
  endif()
  enable_testing()
  add_test(NAME cave_tests COMMAND cave_tests)
endif()
