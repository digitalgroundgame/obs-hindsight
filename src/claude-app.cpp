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

#include "claude-app.hpp"

namespace {

constexpr const char *kDiscordId = "discord";
constexpr const char *kClaudeAppId = "claude_app";

// Tells Claude what it's looking at, the way the API request's system prompt
// does, in a line the streamer can read in the message box.
constexpr const char *kPromptStart =
	"This is an automatic transcript of the last few seconds of my mic while streaming. It may start "
	"mid-sentence and have transcription mistakes. Answer the most recent question I asked, briefly.\n\n";

// Keeps the last max_bytes of text, starting on a whole UTF-8 character.
std::string keep_end(const std::string &text, size_t max_bytes)
{
	if (text.size() <= max_bytes)
		return text;
	size_t start = text.size() - max_bytes;
	while (start < text.size() && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80)
		++start;
	return "\xE2\x80\xA6" + text.substr(start); // …
}

} // namespace

const char *answer_destination_id(AnswerDestination destination)
{
	return destination == AnswerDestination::claude_app ? kClaudeAppId : kDiscordId;
}

AnswerDestination answer_destination_from_id(const std::string &id)
{
	return id == kClaudeAppId ? AnswerDestination::claude_app : AnswerDestination::discord;
}

std::string claude_app_prompt(const std::string &transcript)
{
	// Bytes, not characters: a byte count bounds the characters too, and
	// what is cut is the oldest speech.
	return std::string(kPromptStart) + keep_end(transcript, kClaudeAppMaxTranscript);
}

std::string claude_app_link(const std::string &transcript)
{
	return std::string(kClaudeAppNewChat) + percent_encode(claude_app_prompt(transcript));
}

std::string percent_encode(const std::string &text)
{
	static const char kHex[] = "0123456789ABCDEF";
	std::string encoded;
	encoded.reserve(text.size() * 3);
	for (const char c : text) {
		const auto byte = static_cast<unsigned char>(c);
		if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
		    byte == '-' || byte == '.' || byte == '_' || byte == '~') {
			encoded += c;
		} else {
			encoded += '%';
			encoded += kHex[byte >> 4];
			encoded += kHex[byte & 0x0F];
		}
	}
	return encoded;
}
