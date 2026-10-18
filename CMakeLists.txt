cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
# Asks for a Hindsight code from Digital Ground Game Connect instead of a
# Discord bot token (Tools > Hindsight settings) and Discord user ID (filter
# settings). CI builds turn it on when the HINDSIGHT_ACCESS_CODES repository
# variable is true; local builds can set the environment variable or pass
# -DHINDSIGHT_ACCESS_CODES=ON.
if("$ENV{HINDSIGHT_ACCESS_CODES}")
  set(_hindsight_access_codes_default ON)
else()
  set(_hindsight_access_codes_default OFF)
endif()
option(
  HINDSIGHT_ACCESS_CODES
  "Ask for a Digital Ground Game Connect code in Hindsight settings"
  ${_hindsight_access_codes_default}
)

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
    src/claude-app-open.cpp
    src/claude-app-open.hpp
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
    src/update-install.cpp
    src/update-install.hpp
    src/update-notice.cpp
    src/update-notice.hpp
    ${HINDSIGHT_CORE_SOURCES}
    src/claude-client.cpp
    src/claude-client.hpp
    src/connect-client.cpp
    src/connect-client.hpp
    src/discord-client.cpp
    src/discord-client.hpp
)
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE "${HINDSIGHT_JSON_INCLUDE_DIR}")

if(HINDSIGHT_ACCESS_CODES)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HINDSIGHT_ACCESS_CODES)
endif()

# whisper.cpp for transcription; see src/whisper.cmake.
if(NOT APPLE)
  include(src/whisper.cmake)
  hindsight_add_whisper(${CMAKE_PROJECT_NAME})
endif()

# DPAPI, which secret-seal.cpp uses to encrypt the saved keys; BCrypt, which
# file-hash.cpp uses to check the speech model download and sha256.cpp uses
# to check a downloaded update; and the shell's light API, which
# claude-app-open.cpp uses to see whether the Claude app is installed.
if(WIN32)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE crypt32 bcrypt shlwapi)
endif()

# Where the update notice's Download the update button goes. CI fills this in
# from the HINDSIGHT_UPDATE_URL repository variable; when it is empty, the
# button opens the latest GitHub release (see update-check.cpp). The
# environment always wins over a value cached by an earlier configure.
if(DEFINED ENV{HINDSIGHT_UPDATE_URL})
  set(HINDSIGHT_UPDATE_URL "$ENV{HINDSIGHT_UPDATE_URL}" CACHE STRING "Link for the Download the update button" FORCE)
else()
  set(HINDSIGHT_UPDATE_URL "" CACHE STRING "Link for the Download the update button")
endif()
if(HINDSIGHT_UPDATE_URL)
  # Only characters that pass through a compile definition unchanged on every
  # compiler, so no query strings or fragments.
  if(NOT HINDSIGHT_UPDATE_URL MATCHES "^https://[A-Za-z0-9./_~:-]+$")
    message(
      FATAL_ERROR
      "HINDSIGHT_UPDATE_URL must be an https:// link made of letters, digits and . / _ ~ : -, got: ${HINDSIGHT_UPDATE_URL}"
    )
  endif()
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HINDSIGHT_UPDATE_URL="${HINDSIGHT_UPDATE_URL}")
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
