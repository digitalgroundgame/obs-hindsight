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
  "${CMAKE_CURRENT_LIST_DIR}/clip-file.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/clip-file.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/private-file.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/private-file.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/secret-seal.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/secret-seal.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/secrets.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/secrets.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/version-check.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/version-check.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/wav.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/wav.hpp"
)
