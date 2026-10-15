---
name: discord-api-docs
description: Use before writing or changing Hindsight's Discord delivery code (bot token auth, opening a DM, sending the answer, rate limits) to check it against the Discord HTTP API docs.
---

# Discord API docs

Hindsight sends Claude's answer to the streamer as a Discord DM from the
streamer's own bot (bot token and user ID entered in the plugin). It only needs
the HTTP API, not the Gateway. Check the docs below instead of relying on
memory. Discord moved its docs to docs.discord.com; old
`discord.com/developers/docs/...` links redirect there.

## Sources

- API reference (base URL, versioning, auth header, User-Agent, error format):
  https://docs.discord.com/developers/reference
- Create DM (`POST /users/@me/channels`):
  https://docs.discord.com/developers/resources/user
- Create Message (`POST /channels/{channel.id}/messages`):
  https://docs.discord.com/developers/resources/message
- Rate limits: https://docs.discord.com/developers/topics/rate-limits
- JSON error codes (for example 50007, "Cannot send messages to this user"):
  https://docs.discord.com/developers/topics/opcodes-and-status-codes

## Facts Hindsight depends on

- Base URL `https://discord.com/api/v10`. Always put the version in the path;
  the unversioned default is an old version.
- Headers: `Authorization: Bot <token>` and a User-Agent in the form
  `DiscordBot (https://github.com/digitalgroundgame/obs-hindsight, <version>)`.
  Requests without a valid User-Agent may be blocked.
- Sending a DM is two calls: Create DM with `{"recipient_id": "<user id>"}`
  returns a channel (the existing one if it already exists), then Create
  Message on that channel ID. Cache the channel ID instead of opening it every
  time.
- `content` is at most 2000 characters. Split longer answers into several
  messages, or put the overflow in an embed (6000 characters across embeds).
- Send `"allowed_mentions": {"parse": []}` so text from Claude can never ping
  `@everyone` or anyone else.
- Treat user IDs as strings. They are 64-bit snowflakes and lose precision as
  JSON numbers or doubles.
- On 429, wait `retry_after` (body) or `Retry-After` (header) seconds and retry
  once. Opening many DMs quickly can get a bot rate limited or blocked.
- 50007 means Discord refused the DM. In practice that is usually because the
  bot shares no server with the streamer or the streamer blocks DMs from server
  members. The docs do not list the causes, so tell the streamer what to check
  rather than retrying.
- The bot token is a secret. Never log it or the `Authorization` header.

## When you change this code

- Link the doc section you followed in the PR description.
- If something you rely on is only community knowledge, say so in a code
  comment and in the PR.
