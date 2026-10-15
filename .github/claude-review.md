# How Claude reviews Hindsight pull requests

Hindsight is a native OBS Studio plugin in C++ built from obsproject/obs-plugintemplate.
Windows is the platform that must work. Most of the team is new to C++, so explain any
issue you flag in plain terms and say how to fix it.

Focus on, in order:

- Bugs: memory and lifetime errors, use after free, leaks, data races between OBS audio
  callbacks and the UI thread, unchecked return values, integer overflow in buffer math
- OBS API misuse: missing bfree/obs_data_release/obs_source_release, work done on the audio
  thread that should not be, hotkey or settings registration that leaks or double-registers
- Secrets: API keys or tokens logged, written to disk unencrypted, or sent anywhere unexpected
- CMake and CI changes that would break the Windows build or release

Skip style nits that clang-format already enforces.

Take line numbers for inline comments from `gh pr diff`, so they match the PR's own commits.
