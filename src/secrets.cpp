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

bool is_hindsight_code(const std::string &code)
{
	constexpr size_t kPrefixLength = 3;
	constexpr size_t kRandomLength = 43; // 32 random bytes in base64url, unpadded
	if (code.size() != kPrefixLength + kRandomLength || code.compare(0, kPrefixLength, "hs_") != 0)
		return false;
	for (size_t i = kPrefixLength; i < code.size(); i++) {
		const char c = code[i];
		const bool base64url = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				       c == '-' || c == '_';
		if (!base64url)
			return false;
	}
	return true;
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

bool fill_empty_keys(Secrets &stored, const Secrets &old)
{
	bool changed = false;
	if (stored.claude_api_key.empty())
		changed |= merge_one(stored.claude_api_key, old.claude_api_key);
	if (stored.hindsight_code.empty())
		changed |= merge_one(stored.hindsight_code, old.hindsight_code);
	return changed;
}

bool forget_key(Secrets &secrets, SecretKey key)
{
	std::string &value = key == SecretKey::ClaudeApiKey ? secrets.claude_api_key : secrets.hindsight_code;
	const bool had_value = !value.empty();
	value.clear();
	return had_value;
}

bool merge_typed_keys(Secrets &stored, const Secrets &typed)
{
	const bool claude_changed = merge_one(stored.claude_api_key, typed.claude_api_key);
	const bool code_changed = merge_one(stored.hindsight_code, typed.hindsight_code);
	return claude_changed || code_changed;
}
