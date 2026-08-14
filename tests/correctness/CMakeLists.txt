cmake_minimum_required(VERSION 3.16)
project(KnightriderCorrectness LANGUAGES CXX
        DESCRIPTION "Correctness tests for Knightrider")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

get_filename_component(ENGINE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${ENGINE_DIR}/*.cc")
list(REMOVE_ITEM ENGINE_SOURCES "${ENGINE_DIR}/main.cc")

include(FetchContent)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.12)
FetchContent_MakeAvailable(doctest)

find_package(Threads REQUIRED)

add_executable(perft perft.cc ${ENGINE_SOURCES})
target_include_directories(perft PRIVATE ${ENGINE_DIR})
target_link_libraries(perft PRIVATE doctest::doctest Threads::Threads)

if(NOT MSVC)
  target_compile_options(perft PRIVATE $<$<CONFIG:Release>:-O3>)
endif()
