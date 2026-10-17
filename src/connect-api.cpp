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

#include "connect-api.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <random>

using json = nlohmann::json;

namespace {

std::string string_at(const json &object, const char *key)
{
	const auto found = object.find(key);
	return found != object.end() && found->is_string() ? found->get<std::string>() : std::string();
}

// What to tell the streamer when DGG Connect gives no message of its own.
std::string fallback_message(long status)
{
	if (status == 401)
		return "DGG Connect didn't accept your Hindsight code. Get a new one and paste it in Tools > Hindsight settings.";
	if (status == 429)
		return "Too many messages for now. Wait a minute and try again.";
	if (status >= 500)
		return "DGG Connect couldn't send the message right now. Try again in a minute.";
	return "DGG Connect couldn't send the message (HTTP " + std::to_string(status) + ").";
}

} // namespace

size_t utf16_length(const std::string &text)
{
	size_t units = 0;
	for (const char c : text) {
		const auto byte = static_cast<unsigned char>(c);
		if ((byte & 0xC0) == 0x80)
			continue; // continuation byte
		// A four-byte character is outside the Basic Multilingual Plane, so
		// it takes two UTF-16 units.
		units += byte >= 0xF0 ? 2 : 1;
	}
	return units;
}

std::string connect_message_content(const std::string &question, const std::string &answer)
{
	std::string full = question.empty() ? answer : "**" + question + "**\n" + answer;
	if (utf16_length(full) <= kConnectMaxContent)
		return full;
	// Keep whole characters while they leave room for the one-unit ellipsis.
	size_t units = 0;
	size_t cut = 0;
	for (size_t i = 0; i < full.size(); i++) {
		const auto byte = static_cast<unsigned char>(full[i]);
		if ((byte & 0xC0) == 0x80)
			continue;
		const size_t width = byte >= 0xF0 ? 2 : 1;
		if (units + width + 1 > kConnectMaxContent)
			break;
		units += width;
		cut = i + 1;
		// Take the character's continuation bytes too.
		while (cut < full.size() && (static_cast<unsigned char>(full[cut]) & 0xC0) == 0x80)
			cut++;
	}
	return full.substr(0, cut) + "\xE2\x80\xA6"; // …
}

std::string build_connect_message(const std::string &content, const std::string &id)
{
	// replace, so text that isn't valid UTF-8 gets U+FFFD instead of throwing.
	return json{{"content", content}, {"id", id}}.dump(-1, ' ', false, json::error_handler_t::replace);
}

std::string new_connect_message_id()
{
	std::random_device random;
	std::string id;
	for (int i = 0; i < 4; i++) {
		char part[9];
		std::snprintf(part, sizeof(part), "%08x", static_cast<unsigned>(random()));
		id += part;
	}
	return id;
}

ConnectReply parse_connect_response(long status, const std::string &body)
{
	ConnectReply reply;
	const json parsed = json::parse(body, nullptr, false);
	const bool is_object = parsed.is_object();

	const auto ok = is_object ? parsed.find("ok") : parsed.end();
	if (status >= 200 && status < 300 && is_object && ok != parsed.end() && ok->is_boolean() && ok->get<bool>()) {
		reply.ok = true;
		reply.discord_name = string_at(parsed, "discordName");
		reply.project = string_at(parsed, "project");
		return reply;
	}

	if (is_object) {
		reply.error = string_at(parsed, "error");
		reply.message = string_at(parsed, "message");
	}
	if (reply.error.empty()) {
		if (status == 404) {
			reply.error = "not_found";
			reply.message = "DGG Connect isn't set up to send Hindsight answers yet.";
			return reply;
		}
		reply.error = "bad_response";
	}
	if (reply.message.empty())
		reply.message = fallback_message(status);
	// A server or Discord hiccup; the same id keeps a retry from sending twice.
	reply.retry = status >= 500 || reply.error == "discord_unavailable";
	return reply;
}
