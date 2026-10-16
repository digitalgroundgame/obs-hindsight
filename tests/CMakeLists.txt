# Unit tests for the parts of Hindsight that run without OBS. This is its own
# small project so it builds in seconds, downloading only the four libobs
# headers AudioRing's deque needs instead of all of OBS. The first configure
# needs internet to fetch them; after that they are cached in the build folder
# and the tests build offline:
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
# The headers' SHA-256 at that version, so a moved tag or a tampered download
# fails here instead of being compiled. Update them with the OBS version.
set(_libobs_headers_version 31.1.1)
set(_sha256_base.h ac7518e37fbd34ab6e4a95d27b200ef01d8a894214ee273fa948fcc5a783cf49)
set(_sha256_bmem.h a6a98b93ef6bdabe9bf880d389397846ac2952fcd17a6f9afa332748cff9d52e)
set(_sha256_c99defs.h 1369b34fa066adb7fef43fe3f7dba8d3311ec1148ee8d6ab3fd83eefbecdc830)
set(_sha256_deque.h ef0f9e52e444e3921d0ec3b24902fe3b60d550046ed3b0dd6a279cbac0fa1859)
if(NOT _obs_version VERSION_EQUAL _libobs_headers_version)
  message(
    FATAL_ERROR
    "buildspec.json now uses OBS ${_obs_version}: update the libobs header hashes in tests/CMakeLists.txt"
  )
endif()
foreach(_header IN ITEMS base.h bmem.h c99defs.h deque.h)
  set(_path "${_libobs_headers}/util/${_header}")
  if(NOT EXISTS "${_path}")
    file(
      DOWNLOAD
        "https://raw.githubusercontent.com/obsproject/obs-studio/${_obs_version}/libobs/util/${_header}"
        "${_path}"
      TLS_VERIFY ON
      EXPECTED_HASH SHA256=${_sha256_${_header}}
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
target_include_directories(hindsight-core SYSTEM PUBLIC "${_libobs_headers}" "${HINDSIGHT_JSON_INCLUDE_DIR}")
if(WIN32)
  target_link_libraries(hindsight-core PUBLIC crypt32 bcrypt)
endif()

enable_testing()

set(
  HINDSIGHT_TESTS
  audio-freshness-test
  audio-ring-test
  claude-answer-test
  clip-file-test
  file-sha1-test
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
