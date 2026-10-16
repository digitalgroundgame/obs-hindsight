# Hindsight

An OBS plugin that answers questions asked on stream. The streamer asks a
question out loud, presses a hotkey (or a Stream Deck button), and Hindsight
transcribes the last 30 seconds of their mic on the PC, asks Claude, and posts
the answer to Discord.

Design plan: [#1](https://github.com/digitalgroundgame/obs-hindsight/issues/1).

## Installing

Windows, OBS 31.1 or newer:

1. Download `obs-hindsight-<version>-windows-x64.zip` from the
   [latest release](https://github.com/digitalgroundgame/obs-hindsight/releases/latest).
2. Close OBS.
3. Extract the zip. It holds one folder, `obs-hindsight`.
4. Move that folder into `C:\ProgramData\obs-studio\plugins\` (create
   `plugins` if it does not exist). You should end up with
   `C:\ProgramData\obs-studio\plugins\obs-hindsight\bin\64bit\obs-hindsight.dll`.
   `ProgramData` is hidden by default; paste the path into the Explorer
   address bar to get there.
5. Start OBS. **Help > Log Files > View Current Log** should contain
   `[obs-hindsight] plugin loaded successfully`.

To update, replace the folder with the new one. To uninstall, delete it.

## Trying the current build

This is an early test build. It does not transcribe, call Claude or message
Discord yet; it proves the audio capture.

1. [Install the plugin](#installing).
2. Open your mic's **Filters**, add **Hindsight**, and close the window.
3. In **Settings > Hotkeys**, bind "Hindsight: answer the last question".
4. Talk for a bit, then press the hotkey (or click **Save the last clip now**
   in the filter's settings).
5. **Help > Log Files > View Current Log** shows where the WAV was saved. Play
   it back: it should be the last 30 seconds of your mic.

## Update notices

Each time OBS starts, Hindsight asks GitHub once for the latest release. If a
newer one is out, the top of the Hindsight filter's settings says so, with a
**Download the update** button, and the OBS log notes it. The request goes to
`api.github.com` and carries only the plugin version. If GitHub cannot be
reached, Hindsight carries on without a notice.

## Building

This repo is based on the official
[OBS plugin template](https://github.com/obsproject/obs-plugintemplate).
Supported build environments, the quick start guide and the GitHub Actions
release workflow are documented in the
[template's wiki](https://github.com/obsproject/obs-plugintemplate/wiki).

| Platform     | Tool                  |
| ------------ | --------------------- |
| Windows      | Visual Studio 17 2022 |
| macOS        | Xcode 16.0            |
| Windows, macOS | CMake 3.30.5        |
| Ubuntu 24.04 | CMake 3.28.3, `ninja-build`, `pkg-config`, `build-essential` |

## Checks

Every pull request runs these on GitHub Actions:

- **Builds** on Windows, macOS and Ubuntu with warnings treated as errors.
- **Formatting** with clang-format (C/C++) and gersemi (CMake).
- **Unit tests** for the code that does not need OBS (the audio buffer, the
  WAV encoder and the version comparison), once plain and once under AddressSanitizer and
  UndefinedBehaviorSanitizer, which catch memory bugs and undefined behavior.
- **clang-tidy**, with the bug-finding rules listed in [.clang-tidy](.clang-tidy).

To run the unit tests locally (no OBS needed):

```sh
cmake -S tests -B build_tests
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

Code that OBS does not need to run, like the audio buffer, goes in its own
file under `src/` without OBS includes, so it can be tested this way.

## License

GPL-2.0, as required for plugins that link against OBS. See [LICENSE](LICENSE).

## Releasing

Every merge to `main` publishes a GitHub release for the `version` in
`buildspec.json`, with the Windows, macOS and Linux downloads attached. If that
version was already released, the merge builds but publishes nothing, so bump
the version in the PR that should ship. A version like `0.2.0-beta1` or
`0.2.0-rc1` is published as a pre-release.

Pushing a version tag by hand still works the way the template intended: it
creates a draft release to review and publish yourself.
