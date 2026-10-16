/*
Hindsight
Copyright (C) 2026 Digital Ground Game

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
	// Links the plugin to the streamer's Discord account on the DGG server,
	// which sends them the answers.
	std::string hindsight_code;
};

// The names used in the file and in 0.1.0's filter settings.
constexpr const char *kClaudeApiKeyName = "claude_api_key";
constexpr const char *kHindsightCodeName = "hindsight_code";
// Saved by 0.1.0 and 0.2.0, before answers went through the DGG server. Only
// read so it can be erased.
constexpr const char *kDiscordBotTokenName = "discord_bot_token";

// Drops every whitespace character. Keys never contain any, and pasted keys
// often carry a stray space or newline.
std::string clean_key(const std::string &key);

// Whether a cleaned code has the DGG server's format: "hs_" followed by 43
// base64url characters. Catches a code pasted short or the wrong thing pasted.
bool is_hindsight_code(const std::string &code);

// Which key, for forgetting one at a time.
enum class SecretKey { ClaudeApiKey, HindsightCode };

// Copies each key the streamer typed over the stored one, skipping blank
// fields so an empty box never erases a saved key. Returns whether anything
// changed.
bool merge_typed_keys(Secrets &stored, const Secrets &typed);

// Copies keys from 0.1.0's filter settings into slots that are still empty.
// A key already saved wins, so an old scene collection never replaces a key
// entered since. Returns whether anything changed.
bool fill_empty_keys(Secrets &stored, const Secrets &old);

// Clears one key. Returns whether it held anything.
bool forget_key(Secrets &secrets, SecretKey key);
