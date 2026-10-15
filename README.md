# Hindsight

An OBS plugin that answers questions asked on stream. The streamer asks a
question out loud, presses a hotkey (or a Stream Deck button), and Hindsight
transcribes the last 30 seconds of their mic on the PC, asks Claude, and posts
the answer to Discord.

Design plan: [#1](https://github.com/digitalgroundgame/obs-hindsight/issues/1).

## Trying the current build

This is an early test build. It does not transcribe, call Claude or message
Discord yet; it proves the audio capture.

1. Install the plugin and restart OBS.
2. Open your mic's **Filters**, add **Hindsight**, and close the window.
3. In **Settings > Hotkeys**, bind "Hindsight: answer the last question".
4. Talk for a bit, then press the hotkey (or click **Save the last clip now**
   in the filter's settings).
5. **Help > Log Files > View Current Log** shows where the WAV was saved. Play
   it back: it should be the last 30 seconds of your mic.

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
