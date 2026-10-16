# Unit tests for the parts of Hindsight that run without OBS. This is its own
# small project so it builds in seconds, downloading only the four libobs
# headers AudioRing's deque needs instead of all of OBS:
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

# AudioRing keeps its samples in libobs's header-only deque. Fetch those
# headers at the OBS version the plugin builds against (buildspec.json);
# bmem-shim.cpp stands in for libobs's allocator.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../buildspec.json" _buildspec)
string(JSON _obs_version GET "${_buildspec}" dependencies obs-studio version)
set(_libobs_headers "${CMAKE_CURRENT_BINARY_DIR}/libobs-${_obs_version}")
foreach(_header IN ITEMS base.h bmem.h c99defs.h deque.h)
  set(_path "${_libobs_headers}/util/${_header}")
  if(NOT EXISTS "${_path}")
    file(
      DOWNLOAD
        "https://raw.githubusercontent.com/obsproject/obs-studio/${_obs_version}/libobs/util/${_header}"
        "${_path}"
      TLS_VERIFY ON
      STATUS _status
    )
    list(GET _status 0 _code)
    if(NOT _code EQUAL 0)
      file(REMOVE "${_path}")
      message(FATAL_ERROR "Could not download libobs/util/${_header} for OBS ${_obs_version}: ${_status}")
    endif()
  endif()
endforeach()

include(../src/hindsight-core.cmake)

add_library(hindsight-core STATIC ${HINDSIGHT_CORE_SOURCES} bmem-shim.cpp)
target_include_directories(hindsight-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_include_directories(hindsight-core SYSTEM PUBLIC "${_libobs_headers}")
if(WIN32)
  target_link_libraries(hindsight-core PUBLIC crypt32)
endif()

enable_testing()

foreach(
  _test
  IN
  ITEMS audio-freshness-test audio-ring-test secret-seal-test secrets-test wav-test
)
  add_executable(${_test} ${_test}.cpp)
  target_link_libraries(${_test} PRIVATE hindsight-core)
  add_test(NAME ${_test} COMMAND ${_test})
endforeach()
