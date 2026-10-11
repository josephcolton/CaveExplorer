cmake_minimum_required(VERSION 3.24)
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
  GIT_TAG release-3.2.30
  GIT_SHALLOW TRUE)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)

FetchContent_MakeAvailable(SDL3 json)

# --- Game -------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
add_executable(cave_explorer ${GAME_SOURCES})
target_include_directories(cave_explorer PRIVATE src)
target_link_libraries(cave_explorer PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json)

if(MSVC)
  target_compile_options(cave_explorer PRIVATE /W4 /permissive-)
else()
  target_compile_options(cave_explorer PRIVATE -Wall -Wextra)
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
          ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:cave_explorer>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:cave_explorer>/data
  COMMENT "Copying game content")
add_dependencies(cave_explorer copy_content)
