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
#include <cstddef>

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

std::string encode_secrets(const Secrets &secrets)
{
	std::string text;
	text += kClaudeApiKeyName;
	text += '=' + clean_key(secrets.claude_api_key) + '\n';
	text += kDiscordBotTokenName;
	text += '=' + clean_key(secrets.discord_bot_token) + '\n';
	return text;
}

Secrets decode_secrets(const std::string &text)
{
	Secrets secrets;
	size_t start = 0;
	while (start < text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		const std::string line = text.substr(start, end - start);
		start = end + 1;

		const size_t equals = line.find('=');
		if (equals == std::string::npos)
			continue;
		const std::string name = clean_key(line.substr(0, equals));
		const std::string value = clean_key(line.substr(equals + 1));
		if (name == kClaudeApiKeyName)
			secrets.claude_api_key = value;
		else if (name == kDiscordBotTokenName)
			secrets.discord_bot_token = value;
	}
	return secrets;
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

bool merge_typed_keys(Secrets &stored, const Secrets &typed)
{
	const bool claude_changed = merge_one(stored.claude_api_key, typed.claude_api_key);
	const bool discord_changed = merge_one(stored.discord_bot_token, typed.discord_bot_token);
	return claude_changed || discord_changed;
}
