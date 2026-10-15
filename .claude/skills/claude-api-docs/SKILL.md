---
name: claude-api-docs
description: Use before writing or changing the code in Hindsight that sends the transcribed question to Claude (Messages API over raw HTTP from C++), to check models, headers, errors and response shape against current docs.
---

# Claude API docs

Hindsight sends the transcribed question to the Claude Messages API. There is no
official C++ SDK, so the plugin calls the HTTP API directly. The API changes
often (models, thinking settings, effort), so check the docs below instead of
relying on memory. If your session also has the built-in `claude-api` skill,
load it as well; it is kept current and covers raw HTTP under `curl/`.

## Sources

- Messages API reference (request and response fields):
  https://platform.claude.com/docs/en/api/messages
- Quickstart with a raw HTTP example:
  https://platform.claude.com/docs/en/get-started
- Models, IDs and default effort per model:
  https://platform.claude.com/docs/en/about-claude/models/overview
- Errors, including 429 and 529 and the `request-id` header:
  https://platform.claude.com/docs/en/api/errors
- Rate limits and the `retry-after` header:
  https://platform.claude.com/docs/en/api/rate-limits
- Effort (how hard the model thinks, which drives latency and cost):
  https://platform.claude.com/docs/en/build-with-claude/effort
- Pricing: https://platform.claude.com/docs/en/about-claude/pricing

## Facts Hindsight depends on

- `POST https://api.anthropic.com/v1/messages` with headers `x-api-key`,
  `anthropic-version: 2023-06-01` and `content-type: application/json`. Required
  body fields are `model`, `max_tokens` and `messages`.
- Model choice is tadjh's call; until then use the models page's recommended
  default. Use model IDs exactly as that page lists them, with no date suffix.
  Re-check the page for whether the model allows turning thinking off and what
  its default effort is before relying on either. As of 2026-10-15 the default
  was `claude-opus-5-5`, which always thinks and defaults to `medium` effort.
  For a fast on-stream answer, set `output_config.effort` explicitly rather
  than relying on the default.
- The response `content` is an array of blocks and can include `thinking`
  blocks. Concatenate only the `type == "text"` blocks. Parse JSON with a real
  JSON parser, never string matching.
- Check `stop_reason` before using the text. `max_tokens` means the answer was
  cut off, and `refusal` means there is no answer to send.
- Retry 429, 500, 529 and connection errors with backoff, honouring
  `retry-after` when present. Do not retry 400, 401 or 403; show the streamer a
  clear message (401 almost always means a bad API key).
- Log the `request-id` response header on failures.
- The API key is a secret: follow the Secrets section of `obs-plugin-docs`.

## When you change this code

- Link the doc section you followed in the PR description.
- If a model ID, header or field you need is not on the pages above, say so
  and mark it unverified rather than guessing.
