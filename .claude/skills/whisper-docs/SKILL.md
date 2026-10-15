---
name: whisper-docs
description: Use before writing or changing Hindsight's speech-to-text code (whisper.cpp, models, resampling mic audio for Whisper) to check the whisper.cpp C API at the version we vendor.
---

# whisper.cpp docs

Hindsight transcribes the buffered mic audio locally with
[whisper.cpp](https://github.com/ggml-org/whisper.cpp). Do not write whisper
calls from memory: the C API changes between releases (the old
`whisper_init_from_file` is gone in favour of `..._with_params`, for example).

## Which source wins

1. **`include/whisper.h` at the tag we build against.** Once whisper.cpp is a
   dependency, its version is pinned in this repo (look in `buildspec.json`,
   `CMakeLists.txt` or `cmake/`); use that tag. Until then, pick the latest
   release tag and pin it in the same PR. v1.9.5 was the latest when this skill
   was written: https://github.com/ggml-org/whisper.cpp/blob/v1.9.5/include/whisper.h
2. **The examples at that tag.** `examples/cli/cli.cpp` is the full reference
   for loading a model and running `whisper_full`; `examples/stream/` shows
   live mic transcription:
   https://github.com/ggml-org/whisper.cpp/tree/v1.9.5/examples
3. **The README** for build flags (CUDA, Vulkan, Metal) and quantization:
   https://github.com/ggml-org/whisper.cpp#readme
4. **Models README** for model names, sizes and where to download them:
   https://github.com/ggml-org/whisper.cpp/blob/v1.9.5/models/README.md

To read them locally without the web, set `WHISPER_TAG` to the pinned tag
(find it with `rg -n 'whisper' buildspec.json CMakeLists.txt cmake`), or to the
latest release tag if nothing is pinned yet:

```sh
WHISPER_TAG=v1.9.5  # replace with the pinned tag
WHISPER_SRC=$(mktemp -d)/whisper.cpp
git clone --depth 1 --filter=blob:none --sparse --branch "$WHISPER_TAG" \
  https://github.com/ggml-org/whisper.cpp "$WHISPER_SRC"
git -C "$WHISPER_SRC" sparse-checkout set include examples models
```

## Facts Hindsight depends on

- **Input is 16 kHz mono float PCM** (`WHISPER_SAMPLE_RATE` is 16000 in
  `whisper.h`). OBS mixes at 48 kHz (sometimes 44.1 kHz) and Hindsight's ring
  buffer stores mono at the OBS rate, so resample before calling
  `whisper_full`. libobs already has a resampler:
  https://docs.obsproject.com/reference-libobs-media-io#c.audio_resampler_create
- **Calls you will need:** `whisper_context_default_params`,
  `whisper_init_from_file_with_params`, `whisper_full_default_params`,
  `whisper_full`, `whisper_full_n_segments`, `whisper_full_get_segment_text`,
  `whisper_free`. Check each signature in the header.
- **Threading:** `whisper_full` is "not thread safe for same context" (header
  comment). Use one context per worker thread, or a mutex. It is slow and
  blocking, so never call it from the OBS audio thread, a hotkey callback or the
  UI thread; run it on Hindsight's own worker thread.
- **Load the model once** (it is hundreds of MB) and keep the context for the
  life of the filter or module, not per question.
- **Models** are `ggml-*.bin` files. `.en` models are English-only and faster;
  `-q5_0` and similar are quantized and smaller. The streamer should not have
  to download one by hand; decide in a PR how the plugin gets it.
- **GPU** support is a build flag (`GGML_CUDA`, `GGML_VULKAN`), which changes
  what we ship on Windows. Treat it as a decision for tadjh, not a default.

## When you change transcription code

- Name the header lines or example you followed in the PR description.
- If you bump the whisper.cpp tag, diff `include/whisper.h` between the old and
  new tags and note any signature changes.
- If you cannot reach a source, say so and mark the call as unverified.
