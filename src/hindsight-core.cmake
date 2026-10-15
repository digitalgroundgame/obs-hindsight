# The parts of Hindsight that do not need OBS. Both the plugin (CMakeLists.txt)
# and the unit tests (tests/CMakeLists.txt) build these, so a new OBS-free file
# only needs adding here.

set(
  HINDSIGHT_CORE_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/base64.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/base64.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/audio-ring.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/audio-ring.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/secret-seal.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/secret-seal.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/secrets.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/secrets.hpp"
  "${CMAKE_CURRENT_LIST_DIR}/wav.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/wav.hpp"
)
