cmake_minimum_required(VERSION 3.21)
project(jobeval VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(JOBEVAL_BUILD_TESTS "Build the evalcore unit tests" ON)

add_library(evalcore STATIC
  evalcore/src/decimal.cpp
  evalcore/src/json.cpp
  evalcore/src/value.cpp
  evalcore/src/expr.cpp
  evalcore/src/pack.cpp
  evalcore/src/evaluator.cpp
  evalcore/src/golden.cpp)
target_include_directories(evalcore PUBLIC evalcore/include)
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
  add_test(NAME evalcore_tests COMMAND evalcore_tests)
endif()
