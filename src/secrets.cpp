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

#include "secrets.hpp"

#include <cctype>

std::string clean_key(const std::string &key)
{
	std::string cleaned;
	cleaned.reserve(key.size());
	for (char c : key) {
		if (!std::isspace(static_cast<unsigned char>(c)))
			cleaned.push_back(c);
	}
	return cleaned;
}

namespace {

bool merge_one(std::string &stored, const std::string &typed)
{
	const std::string cleaned = clean_key(typed);
	if (cleaned.empty() || cleaned == stored)
		return false;
	stored = cleaned;
	return true;
}

} // namespace

bool forget_key(Secrets &secrets, SecretKey key)
{
	std::string &value = key == SecretKey::ClaudeApiKey ? secrets.claude_api_key : secrets.discord_bot_token;
	const bool had_value = !value.empty();
	value.clear();
	return had_value;
}

bool merge_typed_keys(Secrets &stored, const Secrets &typed)
{
	const bool claude_changed = merge_one(stored.claude_api_key, typed.claude_api_key);
	const bool discord_changed = merge_one(stored.discord_bot_token, typed.discord_bot_token);
	return claude_changed || discord_changed;
}
