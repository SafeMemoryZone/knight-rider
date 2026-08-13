cmake_minimum_required(VERSION 3.16)
project(Knightrider LANGUAGES CXX
        DESCRIPTION "A lightweight chess engine written in C++")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SANITIZE "ASan + UBSan in Debug" OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cc")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

include(CheckIPOSupported)
check_ipo_supported(RESULT HAVE_LTO)
if(HAVE_LTO)
  set_property(TARGET ${PROJECT_NAME}
               PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE
    /W4 /permissive- /GR-
    $<$<CONFIG:Debug>:/Zi /Od>
    $<$<CONFIG:Release>:/O2>)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -fno-rtti
    $<$<CONFIG:Debug>:-g3 -O0 -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-O3>)

  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-march=native HAVE_NATIVE)
  if(HAVE_NATIVE)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:-march=native>)
  endif()

  if(SANITIZE)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:-fsanitize=undefined>)
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:-fsanitize=undefined>)
  endif()
endif()

message(STATUS "${PROJECT_NAME}: ${CMAKE_BUILD_TYPE}, ${CMAKE_CXX_COMPILER_ID}, LTO=${HAVE_LTO}")
message("${SANITIZE}")
