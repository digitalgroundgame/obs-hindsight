# Hindsight

An OBS plugin that answers questions asked on stream. The streamer asks a
question out loud, presses a hotkey (or a Stream Deck button), and Hindsight
transcribes the last 30 seconds of their mic on the PC, asks Claude, and posts
the answer to Discord.

Design plan: [#1](https://github.com/digitalgroundgame/obs-hindsight/issues/1).

## Installing

Windows, OBS 31.1 or newer:

1. Download `obs-hindsight-<version>-windows-x64-installer.exe` from the
   [latest release](https://github.com/digitalgroundgame/obs-hindsight/releases/latest).
2. Close OBS.
3. Double-click the installer and work through the warnings below, then click
   **Install**.
4. Start OBS. **Help > Log Files > View Current Log** should contain
   `[obs-hindsight] plugin loaded successfully`.

The first time OBS starts with Hindsight, it downloads the speech model (about
142 MB) from Hugging Face in the background. Speech is transcribed on your PC
with [whisper.cpp](https://github.com/ggml-org/whisper.cpp); your audio is
never uploaded. Transcription needs a CPU from about 2015 or newer (one with
AVX2).

### Windows warnings you will see

The installer is not code-signed yet, so Windows does not know who made it and
warns you before running it. These warnings are expected. Only click past them
for an installer you downloaded from the release page linked above.

- **Your browser may hold the download back** with a message like "isn't
  commonly downloaded". In Edge, open the **...** menu next to the download,
  choose **Keep**, then **Show more** and **Keep anyway**. In Chrome, choose
  **Keep**.
- **"Windows protected your PC"** (a blue SmartScreen window) appears when you
  open the installer. Click **More info**, check that the app is
  `obs-hindsight-<version>-windows-x64-installer.exe`, then click
  **Run anyway**.
- **"Do you want to allow this app from an unknown publisher to make changes
  to your device?"** asks for administrator rights, because the plugin goes in
  the shared OBS plugins folder. Click **Yes**.

To check the file is the one we published, compare its SHA-256 (in PowerShell:
`Get-FileHash .\obs-hindsight-<version>-windows-x64-installer.exe`) with the
checksum listed on the release page.

### Updating and uninstalling

On Windows, the easiest way to update is the **Update now** button in the
Hindsight filter's settings (see [Update notices](#update-notices)). You can
also run the newer installer the same way as the first one. Either way it
replaces the old version and keeps your settings and keys. If OBS is open, the
installer offers to close it, and its last page can start OBS again.

To uninstall, open Windows **Settings > Apps > Installed apps**, find
**Hindsight for OBS**, and choose **Uninstall**.

### Installing by hand

If you would rather not run an installer, download
`obs-hindsight-<version>-windows-x64.zip` instead, close OBS, extract it, and
move the `obs-hindsight` folder into `C:\ProgramData\obs-studio\plugins\`
(create `plugins` if it does not exist). You should end up with
`C:\ProgramData\obs-studio\plugins\obs-hindsight\bin\64bit\obs-hindsight.dll`.
`ProgramData` is hidden by default; paste the path into the Explorer address
bar to get there. To update, replace the folder with the new one. To
uninstall, delete it.

## Trying the current build

<!-- The Windows installer links to this heading (#trying-the-current-build).
If you rename it, update the link in cmake/windows/resources/installer-Windows.iss. -->

This is an early test build. The hotkey transcribes the last 30 seconds of your
mic on your PC, asks Claude for the question and answer, and writes them to the
OBS log. It does not message Discord yet.

1. [Install the plugin](#installing) and start OBS. Wait a minute or two for
   the speech model to download; the log says `speech model downloaded` when
   it's done.
2. Under **Tools > Hindsight settings**, enter your Claude API key and click
   **Save**.
3. Open your mic's **Filters**, add **Hindsight**, and close the window.
4. In **Settings > Hotkeys**, find your mic's name and bind "Hindsight: answer the
   last question" under it.
5. Ask a question out loud, then press the hotkey (or click **Save the last
   clip now** in the filter's settings).
6. **Help > Log Files > View Current Log** shows what Hindsight heard, the
   question Claude found and its answer, with timings, plus where the clip was
   saved as a WAV. Because the log holds what you said, check it before
   sharing it publicly.

## Update notices

Each time OBS starts, Hindsight asks GitHub once for the latest release. If a
newer one is out, the top of the Hindsight filter's settings says so and the
OBS log notes it. If GitHub cannot be reached, Hindsight carries on without a
notice.

On Windows the notice has an **Update now** button. It downloads the new
installer, checks its SHA-256 against the one GitHub lists for the release, and
opens it. From there it is the usual install: click **Yes** on the Windows
prompt, then **Install**. The installer closes OBS for you, and its last page
offers to start OBS again. The button waits while you are streaming or
recording, since the update closes OBS. If the download or the check fails, the
notice says so and offers the release page instead. On other systems the notice
has a **Download the update** button that opens the release page.

### What the update check sends

One HTTPS GET to
`https://api.github.com/repos/digitalgroundgame/obs-hindsight/releases/latest`
with `User-Agent: obs-hindsight/<version>` (GitHub requires one) and an
`Accept` header asking for JSON. Nothing else: no keys, no audio, no details
about you or your PC. Like any web request, it shows GitHub your IP address.
**Update now** adds one more HTTPS download, of the installer file itself, only
when you click it.

### Turning it off

Open **Tools > Hindsight settings**, untick **Check for updates when OBS
starts**, and click **Save**. The choice is kept in `settings.json` in
Hindsight's config folder, apart from your keys. Ticking it again takes effect
the next time OBS starts.

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
- **Unit tests** for the code that does not need OBS (the audio buffer and its
  freshness checks, the WAV encoder, the saved keys, the version comparison and
  the request to Claude and how its reply is read)
  on Ubuntu, once plain and once under AddressSanitizer and
  UndefinedBehaviorSanitizer, which catch memory bugs and undefined behavior.
  They also run on Windows, the only place the key encryption code runs.
- **clang-tidy**, with the bug-finding rules listed in [.clang-tidy](.clang-tidy).

To run the unit tests locally (no OBS needed):

```sh
cmake -S tests -B build_tests
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

Code that OBS does not need to run, like the WAV encoder, goes in its own
file under `src/` without OBS includes, so it can be tested this way. The audio
buffer is the one exception: it uses libobs's header-only `util/deque.h`, which
the test build downloads for the OBS version in `buildspec.json`. JSON is read
and written with [nlohmann/json](https://github.com/nlohmann/json), which both
the plugin and test builds download, pinned by hash. So the first configure
needs internet access.

## License

GPL-2.0, as required for plugins that link against OBS. See [LICENSE](LICENSE).

## Releasing

Every merge to `main` publishes a GitHub release for the `version` in
`buildspec.json`, with the Windows, macOS and Linux downloads attached. If that
version was already released, the merge builds but publishes nothing, so bump
the version in the PR that should ship. The release waits for the build,
formatting, unit tests and clang-tidy, and is skipped if any of them fail. A
version like `0.2.0-beta1` or `0.2.0-rc1` is published as a pre-release.

Pushing a version tag by hand still works the way the template intended: it
creates a draft release to review and publish yourself.
