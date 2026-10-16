---
name: obs-plugin-docs
description: Use before writing or reviewing OBS plugin code in this repo (libobs, filters, audio, hotkeys, properties, settings, module load, logging, obs-plugintemplate) to check APIs against the OBS version we actually build against.
---

# OBS plugin docs

Hindsight is a native OBS plugin built on `obsproject/obs-plugintemplate`. libobs
is a C API with sparse docs and some behaviour that only shows up in the headers,
so do not write plugin code from memory. Before adding or changing a call into
libobs, look it up in the sources below and match the signature, ownership
(who frees what) and threading rules you find there.

## Which source wins

1. **libobs headers at the version we build against.** The version is
   `dependencies.obs-studio.version` in `buildspec.json` (31.1.1 when this skill
   was written). The headers are the ground truth for signatures and comments.
   Links below point at the `31.1.1` tag; swap the tag if `buildspec.json` moves.
2. **docs.obsproject.com.** Good prose for the big structs and callbacks, but it
   tracks the newest OBS (33.0 when we were on 31.1.1), so it can describe API
   we do not have yet. The page footer says which version it documents.
   Some areas (hotkeys, most of `obs-frontend-api` usage) are barely documented.
3. **Built-in OBS plugins** in `obsproject/obs-studio/plugins/`. When the docs
   are thin, find how OBS's own plugins do it.
4. **The plugin template** for anything about CMake, `buildspec.json`, CI and
   packaging. The commit we imported is in `.github/template-version`.
5. **The OBS plugins guide** for where users install plugins on each OS:
   https://obsproject.com/kb/plugins-guide

If two sources disagree, trust the header at our tag, and say so in the PR.

## Getting the sources locally

The docs site can be unreachable from cloud sessions; the GitHub clone works.
A sparse, shallow clone of just the headers, docs and example filters is small:

```sh
OBS_VERSION=$(jq -r '.dependencies["obs-studio"].version' buildspec.json)
OBS_SRC=$(mktemp -d)/obs-studio
git clone --depth 1 --filter=blob:none --sparse --branch "$OBS_VERSION" \
  https://github.com/obsproject/obs-studio "$OBS_SRC"
git -C "$OBS_SRC" sparse-checkout set libobs docs/sphinx plugins/obs-filters
```

Then grep it, for example `rg -n 'obs_hotkey_register_source' "$OBS_SRC/libobs"`
or `rg -n 'filter_audio' "$OBS_SRC/docs/sphinx"`. Keep the clone out of
this repo. If you can only reach the web, fetch the doc pages below.

## Starting points

- Plugin guide (module layout, registering sources, settings, properties,
  localization): https://docs.obsproject.com/plugins
- Backend design, including which libobs thread does what:
  https://docs.obsproject.com/backend-design
- Template README (build, CMake presets, `buildspec.json`):
  https://github.com/obsproject/obs-plugintemplate
- Template wiki (build system, distributing, "How To Debug Your Plugin"):
  https://github.com/obsproject/obs-plugintemplate/wiki

## Cheat sheet for what Hindsight uses

### Module load and unload (`src/plugin-main.cpp`)

- `OBS_DECLARE_MODULE`, `OBS_MODULE_USE_DEFAULT_LOCALE`, `obs_module_load`,
  `obs_module_unload`, `obs_module_post_load`:
  https://docs.obsproject.com/reference-modules
- `obs_module_text` (strings come from `data/locale/en-US.ini`):
  https://docs.obsproject.com/reference-modules#c.obs_module_text
- `obs_module_config_path` returns memory you must `bfree`, and can return NULL,
  so check it before use: https://docs.obsproject.com/reference-modules#c.obs_module_config_path
  (Known gap: `capture()` in `src/hindsight-filter.cpp` passes it to `os_mkdirs`
  unchecked. Do not copy that.)
- Header: https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs-module.h

### The audio filter (`src/hindsight-filter.cpp`)

- `struct obs_source_info` and every callback (`get_name`, `create`, `destroy`,
  `update`, `get_defaults`, `get_properties`, `filter_audio`), plus the
  `OBS_SOURCE_TYPE_FILTER` / `OBS_SOURCE_AUDIO` flags:
  https://docs.obsproject.com/reference-sources
- `filter_audio`, including the rule that returned data must stay alive until
  the next call: https://docs.obsproject.com/reference-sources#c.obs_source_info.filter_audio
- `obs_register_source`: https://docs.obsproject.com/reference-sources#c.obs_register_source
- `struct obs_audio_data` is only in the header:
  https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs-source.h
- Real audio filters to copy patterns from (`gain-filter.c` is the smallest):
  https://github.com/obsproject/obs-studio/tree/31.1.1/plugins/obs-filters

### Audio format and threading

- `obs_get_audio`: https://docs.obsproject.com/reference-core#c.obs_get_audio
- `audio_output_get_sample_rate`, `audio_output_get_channels` and the audio
  format enums: https://docs.obsproject.com/reference-libobs-media-io
- Audio is planar float. `filter_audio` runs on the libobs audio thread
  (see "Libobs Threads" in https://docs.obsproject.com/backend-design), while
  `update`, properties callbacks and hotkeys run on other threads. Never block,
  allocate large buffers, or do network or disk I/O in `filter_audio`, and keep
  any lock it shares with other threads held only briefly. Hand slow work to
  another thread. (Known gap: `snapshot()` and `update()` in
  `src/hindsight-filter.cpp` copy or reallocate the whole ring buffer while
  holding the lock `filter_audio` waits on. Do not copy that pattern; swap
  buffers or copy in small chunks instead.)
- Header: https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/media-io/audio-io.h
- Volume meters and faders, if we ever need levels:
  https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs-audio-controls.h

### Hotkeys

docs.obsproject.com has no hotkey reference. Read the header:
https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs-hotkey.h

- `obs_hotkey_register_source(source, name, description, func, data)` ties the
  hotkey to the filter, so OBS saves its binding with the source.
- `obs_hotkey_func` fires on both press and release; check `pressed`.
- Unregister in `destroy` with `obs_hotkey_unregister`.

### Properties and settings

- Properties UI (`obs_properties_add_text`, `OBS_TEXT_PASSWORD`, `OBS_TEXT_INFO`,
  `obs_property_text_set_info_type`, `obs_properties_add_int_slider`,
  `obs_properties_add_button2`): https://docs.obsproject.com/reference-properties
  Use `obs_properties_add_button2`, not `obs_properties_add_button`: only the
  `2` version passes your filter pointer to the click callback.
- Settings data (`obs_data_get_string`, `obs_data_get_int`,
  `obs_data_set_default_int`, reference counting with `obs_data_release`):
  https://docs.obsproject.com/reference-settings
### Secrets

This section is the one place the secret rules live; other skills link here.

- API keys and bot tokens never go in logs, crash reports, error messages or
  the `obs_log` output, and neither do request headers that carry them.
- Filter settings are saved in the scene collection, which streamers export and
  share, so secrets must not live there. Store them in module config
  (`obs_module_config_path`) instead. (Known gap: the filter currently keeps
  `claude_api_key` and `discord_bot_token` in its settings; that has to move
  before a public release.)

### Logging, memory and files

- `blog` and log levels: https://docs.obsproject.com/reference-libobs-util-base#c.blog
  (`obs_log` in `src/plugin-support.h` is the template's wrapper that prefixes
  the plugin name; use it.)
- `bfree` and libobs allocation: https://docs.obsproject.com/reference-libobs-util-bmem
- `os_fopen`, `os_mkdirs` and other portable file helpers:
  https://docs.obsproject.com/reference-libobs-util-platform

## Related skills

For the rest of the pipeline, load `whisper-docs` (transcription),
`claude-api-docs` (asking Claude) and `discord-api-docs` (sending the answer).
For UI and anything built on Qt, load `qt-docs`.

## When you change plugin code

- Name the doc page or header you checked in the PR description for any new
  libobs call.
- Say which thread each callback you touch runs on, and guard shared state.
- Free what libobs hands you as its header says: `bfree` for strings like
  `obs_module_config_path` and `bstrdup`, `obs_data_release` and
  `obs_source_release` for refcounted objects.
- Do not use functions the headers at our tag mark deprecated.
- When the monthly upstream-check issue reports a new OBS version, diff the
  headers we use between the old and new tags before bumping `buildspec.json`.
- If you cannot reach a source, say so and mark the API use as unverified
  rather than guessing.
- If you rely on behaviour that is only in a header comment or a built-in
  plugin, add a one-line code comment linking to it.
- If an upstream page here moves or a section is wrong, fix this skill in the
  same PR.
