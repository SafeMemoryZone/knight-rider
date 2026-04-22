cmake_minimum_required(VERSION 3.16)

project(
  Knightrider
  DESCRIPTION
    "A lightweight chess engine written in C++ with a focus on clean code and solid tactical play"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# default to release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# enable IPO/LTO for Release builds (GCC/Clang/MSVC)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

file(GLOB_RECURSE SOURCES "src/*.cc")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# threads
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

target_compile_options(
  ${PROJECT_NAME}
  PRIVATE # GCC / Clang
          $<$<CXX_COMPILER_ID:GNU,Clang>:
          $<$<CONFIG:Debug>:-g
          -O0
          -Wall
          -Wextra
          -Wpedantic
          -Wshadow
          -Wconversion>
          $<$<CONFIG:Release>:
          -O3
          -ffast-math
          -DNDEBUG
          -march=native
          -Wall
          -Wextra
          -Wpedantic
          -Wshadow
          -Wconversion
          -fstrict-aliasing
          -ffunction-sections
          -fdata-sections
          -fno-rtti
          >
          >
          # MSVC
          $<$<CXX_COMPILER_ID:MSVC>:
          $<$<CONFIG:Debug>:/Zi
          /Od
          /W4
          /permissive->
          $<$<CONFIG:Release>:/O2
          /Ob2
          /DNDEBUG
          /GL>
          >)

message(STATUS "=== Chess Engine Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(
  STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Optimization: -Ofast (Release)")
message(STATUS "IPO/LTO: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}")
message(STATUS "========================================")
