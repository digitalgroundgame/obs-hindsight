/*
Hindsight
Copyright (C) 2026 DGG AC Dev Team

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <string>

// The keys Hindsight needs. They live in their own file in the plugin's config
// folder, never in the filter settings, because OBS saves filter settings into
// the scene collection and streamers export and share those. Kept free of OBS
// so the tests can run it on its own; secret-store.cpp does the file reading
// and writing, and secret-seal.cpp encrypts the file.
struct Secrets {
	std::string claude_api_key;
	std::string discord_bot_token;
};

// The names used both for the filter's key fields and inside the file.
constexpr const char *kClaudeApiKeyName = "claude_api_key";
constexpr const char *kDiscordBotTokenName = "discord_bot_token";

// Drops every whitespace character. Keys never contain any, and pasted keys
// often carry a stray space or newline.
std::string clean_key(const std::string &key);

// Which key, for forgetting one at a time.
enum class SecretKey { ClaudeApiKey, DiscordBotToken };

// One "name=value" line per key. This is the text inside the sealed file.
std::string encode_secrets(const Secrets &secrets);

// Reads what encode_secrets wrote. Blank lines, unknown names and lines with
// no '=' are skipped, so an older or hand-edited file still loads.
Secrets decode_secrets(const std::string &text);

// Copies each key the streamer typed over the stored one, skipping blank
// fields so an empty box never erases a saved key. Returns whether anything
// changed.
bool merge_typed_keys(Secrets &stored, const Secrets &typed);

// Clears one key. Returns whether it held anything.
bool forget_key(Secrets &secrets, SecretKey key);
