cmake_minimum_required(VERSION 3.21)
project(jobeval VERSION 0.7.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(JOBEVAL_BUILD_APP "Build the Qt desktop app (needs Qt 6)" OFF)
option(JOBEVAL_BUILD_TESTS "Build the evalcore unit tests" ON)

# Vendored Monocypher (BSD-2-Clause OR CC0): Ed25519 curve arithmetic and BLAKE2b.
add_library(monocypher STATIC evalcore/third_party/monocypher/monocypher.c)
target_include_directories(monocypher PUBLIC evalcore/third_party/monocypher)
set_target_properties(monocypher PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF)

add_library(evalcore STATIC
  evalcore/src/hash.cpp
  evalcore/src/minisign.cpp
  evalcore/src/catalog.cpp
  evalcore/src/packstore.cpp
  evalcore/src/takehome.cpp
  evalcore/src/realhourly.cpp
  evalcore/src/jobcalc.cpp
  evalcore/src/notworking.cpp
  evalcore/src/scoring.cpp
  evalcore/src/decimal.cpp
  evalcore/src/json.cpp
  evalcore/src/value.cpp
  evalcore/src/expr.cpp
  evalcore/src/pack.cpp
  evalcore/src/evaluator.cpp
  evalcore/src/golden.cpp)
target_include_directories(evalcore PUBLIC evalcore/include)
target_link_libraries(evalcore PUBLIC monocypher)
if(MSVC)
  # Decimal uses a 128-bit integer for exact fixed-point maths (GCC/Clang extension).
  message(FATAL_ERROR "MSVC is not supported: build with GCC (MinGW-w64) or Clang.")
endif()
target_compile_options(evalcore PRIVATE -Wall -Wextra)

add_executable(eval-cli evalcore/cli/main.cpp)
target_link_libraries(eval-cli PRIVATE evalcore)

if(JOBEVAL_BUILD_TESTS)
  enable_testing()
  add_executable(evalcore_tests evalcore/tests/test_main.cpp)
  target_link_libraries(evalcore_tests PRIVATE evalcore)
  target_compile_definitions(evalcore_tests PRIVATE JOBEVAL_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/evalcore/tests/fixtures")
  add_test(NAME evalcore_tests COMMAND evalcore_tests)
endif()

if(JOBEVAL_BUILD_APP)
  add_subdirectory(app)
endif()
