# The parts of Hindsight that run without OBS (AudioRing uses only libobs's
# header-only deque). Both the plugin (CMakeLists.txt) and the unit tests
# (tests/CMakeLists.txt) build these, so a new file of this kind only needs
# adding here.

set(
  HINDSIGHT_CORE_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/audio-freshness.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/audio-freshness.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/audio-ring.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/audio-ring.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-answer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-answer.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-app.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-app.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-models.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-models.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-route.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/claude-route.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/clip-cleanup.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/clip-cleanup.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/clip-file.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/clip-file.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/connect-api.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/connect-api.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/discord-api.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/discord-api.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/file-hash.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/file-hash.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/local-time.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/local-time.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/private-file.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/private-file.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/secret-seal.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/secret-seal.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/secrets.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/secrets.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/sha256.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/sha256.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/update-asset.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/update-asset.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/version-check.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/version-check.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/wav.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/wav.hpp"
)

# nlohmann/json, the single-header JSON library obs-websocket also uses, for
# the JSON the core files read and write. Downloaded at configure time and
# pinned by hash; add HINDSIGHT_JSON_INCLUDE_DIR as a SYSTEM include.
set(HINDSIGHT_JSON_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/nlohmann-json-3.11.3")
set(_hindsight_json "${HINDSIGHT_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
if(NOT EXISTS "${_hindsight_json}")
  file(
    DOWNLOAD "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" "${_hindsight_json}"
    TLS_VERIFY ON
    EXPECTED_HASH SHA256=9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6
    STATUS _hindsight_json_status
  )
  list(GET _hindsight_json_status 0 _hindsight_json_code)
  if(NOT _hindsight_json_code EQUAL 0)
    file(REMOVE "${_hindsight_json}")
    message(FATAL_ERROR "Could not download nlohmann/json 3.11.3: ${_hindsight_json_status}")
  endif()
endif()
