cmake_minimum_required(VERSION 3.16)
project(TheLawOfPrey VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Release when not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# Put all build outputs into <source-root>/build/<Config> (e.g. build/Release, build/Debug)
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# Project executable
add_executable(${PROJECT_NAME}
  src/main.c
  src/graphsettings.c
)

# Headers
target_include_directories(${PROJECT_NAME} PRIVATE
  src/include
)

# Compiler flags per-configuration
if(MSVC)
  # MSVC: use flags appropriate for MSVC if needed
  target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:/Zi /Od>
    $<$<CONFIG:Release>:/O2 /Ot /GL>
  )
else()
  target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
    $<$<CONFIG:Release>:-O3 -march=native -flto -DNDEBUG>
  )
endif()

# Try to find raylib (more robust handling and helpful messages)
# Allow user to hint raylib location with environment variable RAYLIB_DIR or CMake variable RAYLIB_DIR
set(RAYLIB_HINTS $ENV{RAYLIB_DIR} ${RAYLIB_DIR} ${CMAKE_SOURCE_DIR}/external/raylib /usr/local /usr)
# Prefer config/package provided by raylib (provides imported target raylib::raylib)
find_package(raylib CONFIG QUIET HINTS ${RAYLIB_HINTS})

if(TARGET raylib::raylib)
  target_link_libraries(${PROJECT_NAME} PRIVATE raylib::raylib)
else()
  # Try to find headers and library manually as a fallback
  find_path(RAYLIB_INCLUDE_DIR NAMES raylib.h HINTS ${RAYLIB_HINTS} PATH_SUFFIXES include)
  find_library(RAYLIB_LIBRARY NAMES raylib HINTS ${RAYLIB_HINTS} PATH_SUFFIXES lib lib64)
  if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
    message(STATUS "Found raylib (include: ${RAYLIB_INCLUDE_DIR}, lib: ${RAYLIB_LIBRARY})")
    target_include_directories(${PROJECT_NAME} PRIVATE ${RAYLIB_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_LIBRARY})
  else()
    message(FATAL_ERROR "Could not find raylib.\n\nPlease install raylib or point CMake to it.\n\nOptions:\n - Install via package manager:\n   macOS (Homebrew):    brew install raylib\n   Ubuntu/Debian:       sudo apt install libraylib-dev  # package name may vary\n - Build from source:   https://github.com/raysan5/raylib\n - Or set environment variable RAYLIB_DIR or pass -DRAYLIB_DIR=/path/to/raylib when calling cmake.\n\nExamples:\n cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DRAYLIB_DIR=/usr/local\n")
  endif()
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output directory: ${OUTPUT_DIR}")
