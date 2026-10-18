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

#include "discord-api.hpp"

#include <nlohmann/json.hpp>

#include <cctype>

using nlohmann::json;

namespace {

// Discord's JSON error codes Hindsight explains:
// https://docs.discord.com/developers/topics/opcodes-and-status-codes#json
constexpr long kUnknownUser = 10013;
constexpr long kCannotMessageUser = 50007;
constexpr long kInvalidFormBody = 50035;

DiscordReply failed(const char *error, std::string message)
{
	DiscordReply reply;
	reply.error = error;
	reply.message = std::move(message);
	return reply;
}

// An error reply: {"code": 50007, "message": "..."}, plus retry_after on a 429.
DiscordReply parse_error(long status, const json &body)
{
	long code = 0;
	std::string message;
	if (body.is_object()) {
		const auto code_it = body.find("code");
		if (code_it != body.end() && code_it->is_number_integer())
			code = code_it->get<long>();
		const auto message_it = body.find("message");
		if (message_it != body.end() && message_it->is_string())
			message = message_it->get<std::string>();
	}

	if (status == 401)
		return failed("invalid_token",
			      "Discord rejected the bot token. Check it in Tools > Hindsight settings.");
	if (status == 429) {
		DiscordReply reply = failed("rate_limited", "Discord asked Hindsight to slow down. Try again shortly.");
		if (body.is_object()) {
			const auto wait = body.find("retry_after");
			if (wait != body.end() && wait->is_number())
				reply.retry_after = wait->get<double>();
		}
		return reply;
	}
	if (code == kCannotMessageUser)
		return failed("dm_refused",
			      "Discord won't let your bot DM you. Make sure the bot is in a server you're in and that "
			      "you allow DMs from that server's members.");
	if (code == kUnknownUser || code == kInvalidFormBody)
		return failed("bad_user_id",
			      "Discord doesn't know that user ID. Check your Discord user ID in Tools > Hindsight "
			      "settings.");
	if (status >= 500)
		return failed("server", "Discord had a problem (HTTP " + std::to_string(status) + "). Try again.");
	return failed("discord_error", "Discord refused the message (HTTP " + std::to_string(status) +
					       (message.empty() ? ")." : "): " + message));
}

} // namespace

bool is_discord_user_id(const std::string &id)
{
	if (id.size() < 17 || id.size() > 20)
		return false;
	for (const char c : id)
		if (!std::isdigit(static_cast<unsigned char>(c)))
			return false;
	return true;
}

std::string build_discord_open_dm(const std::string &user_id)
{
	return json{{"recipient_id", user_id}}.dump();
}

std::string build_discord_message(const std::string &content)
{
	const json body = {
		{"content", content},
		{"allowed_mentions", {{"parse", json::array()}}},
	};
	return body.dump(-1, ' ', false, json::error_handler_t::replace);
}

DiscordReply parse_discord_open_dm(long status, const std::string &body)
{
	const json reply = json::parse(body, nullptr, false);
	if (status < 200 || status >= 300)
		return parse_error(status, reply);
	if (reply.is_object()) {
		const auto id = reply.find("id");
		if (id != reply.end() && id->is_string() && !id->get<std::string>().empty()) {
			DiscordReply result;
			result.ok = true;
			result.channel_id = id->get<std::string>();
			return result;
		}
	}
	return failed("bad_response", "Discord's reply couldn't be read.");
}

DiscordReply parse_discord_message(long status, const std::string &body)
{
	if (status < 200 || status >= 300)
		return parse_error(status, json::parse(body, nullptr, false));
	DiscordReply result;
	result.ok = true;
	return result;
}
