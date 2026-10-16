cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

# For the update check and the speech model download. OBS ships libcurl on Windows, and obs-deps provides it to build against.
find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

include(src/hindsight-core.cmake)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/answer-pipeline.cpp
    src/answer-pipeline.hpp
    src/hindsight-filter.cpp
    src/hindsight-filter.hpp
    src/plugin-settings.cpp
    src/plugin-settings.hpp
    src/secret-store.cpp
    src/secret-store.hpp
    src/settings-dialog.cpp
    src/settings-dialog.hpp
    src/speech-model.cpp
    src/speech-model.hpp
    src/transcriber.cpp
    src/transcriber.hpp
    src/update-check.cpp
    src/update-check.hpp
    ${HINDSIGHT_CORE_SOURCES}
    src/claude-client.cpp
    src/claude-client.hpp
)
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE "${HINDSIGHT_JSON_INCLUDE_DIR}")

# whisper.cpp for transcription; see src/whisper.cmake.
if(NOT APPLE)
  include(src/whisper.cmake)
  hindsight_add_whisper(${CMAKE_PROJECT_NAME})
endif()

# DPAPI, which secret-seal.cpp uses to encrypt the saved keys, and BCrypt,
# which file-sha1.cpp uses to check the speech model download.
if(WIN32)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE crypt32 bcrypt)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
