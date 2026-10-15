# Hindsight

An OBS plugin that answers questions asked on stream. The streamer asks a
question out loud, presses a hotkey (or a Stream Deck button), and Hindsight
transcribes the last 30 seconds of their mic on the PC, asks Claude, and posts
the answer to Discord.

Design plan: [#1](https://github.com/digitalgroundgame/obs-hindsight/issues/1).
Nothing beyond the plugin skeleton is built yet.

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

## License

GPL-2.0, as required for plugins that link against OBS. See [LICENSE](LICENSE).
