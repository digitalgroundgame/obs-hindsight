# whisper.cpp, which transcribes the streamer's mic on their own PC. Built from
# source as a static library at a pinned release and linked into the plugin.
# Only the CPU backend is built, for x86-64 CPUs with AVX2 (Intel Haswell and
# AMD Excavator, 2013-2015, and newer); transcriber.cpp checks the CPU before
# using it. Not built for macOS yet: the universal (x86 and Arm) build needs
# its own setup.

include(FetchContent)

set(_whisper_version v1.9.5)
set(_whisper_commit d1be6fde11ac6e0407606b4e42fe72d34add8037)

# A function, so these settings stay out of the plugin's own scope.
function(hindsight_add_whisper target)
  # Lets these plain variables set the projects' option()s.
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  set(BUILD_SHARED_LIBS OFF)
  set(WHISPER_BUILD_TESTS OFF)
  set(WHISPER_BUILD_EXAMPLES OFF)
  set(WHISPER_BUILD_SERVER OFF)
  set(WHISPER_CURL OFF)
  set(WHISPER_SDL2 OFF)
  # Portable x86-64 code instead of code tuned to the build machine.
  set(GGML_NATIVE OFF)
  set(GGML_AVX ON)
  set(GGML_AVX2 ON)
  set(GGML_FMA ON)
  set(GGML_F16C ON)
  set(GGML_AVX512 OFF)
  # OpenMP would need its runtime shipped with the plugin; ggml's own thread
  # pool does the same job.
  set(GGML_OPENMP OFF)
  set(GGML_CCACHE OFF)
  # Third-party code: its warnings are not ours to fail the build on.
  set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)

  FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG ${_whisper_version}
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(whisper)

  # The tag is checked against its commit, so a moved tag fails here instead
  # of being built.
  find_package(Git REQUIRED)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${whisper_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE _head
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT _head STREQUAL _whisper_commit)
    message(FATAL_ERROR "whisper.cpp ${_whisper_version} is commit ${_head}, expected ${_whisper_commit}")
  endif()

  target_link_libraries(${target} PRIVATE whisper)
  target_compile_definitions(${target} PRIVATE HINDSIGHT_HAS_WHISPER=1)
endfunction()
