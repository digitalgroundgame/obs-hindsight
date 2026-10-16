# Unit tests for the parts of Hindsight that do not need OBS. This is its own
# small project so it builds in seconds without downloading OBS:
#
#   cmake -S tests -B build_tests
#   cmake --build build_tests
#   ctest --test-dir build_tests --output-on-failure
#
# Add -DHINDSIGHT_SANITIZE=ON to run them under AddressSanitizer and
# UndefinedBehaviorSanitizer (GCC or Clang).

cmake_minimum_required(VERSION 3.28...3.30)

project(hindsight-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HINDSIGHT_SANITIZE "Build the tests with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Werror)
endif()

if(HINDSIGHT_SANITIZE AND MSVC)
  message(FATAL_ERROR "HINDSIGHT_SANITIZE needs GCC or Clang")
elseif(HINDSIGHT_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

include(../src/hindsight-core.cmake)

add_library(hindsight-core STATIC ${HINDSIGHT_CORE_SOURCES})
target_include_directories(hindsight-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
if(WIN32)
  target_link_libraries(hindsight-core PUBLIC crypt32)
endif()

enable_testing()

set(
  HINDSIGHT_TESTS
  audio-freshness-test
  audio-ring-test
  private-file-test
  secret-seal-test
  secrets-test
  version-check-test
  wav-test
)

foreach(_test IN LISTS HINDSIGHT_TESTS)
  add_executable(${_test} ${_test}.cpp)
  target_link_libraries(${_test} PRIVATE hindsight-core)
  add_test(NAME ${_test} COMMAND ${_test})
endforeach()
