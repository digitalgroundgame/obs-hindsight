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

// What Hindsight sends to Discord to DM the answer from the streamer's own
// bot, and how it reads the replies. Kept free of OBS and curl so the tests
// can run it on its own; discord-client.cpp does the sending. Builds with
// HINDSIGHT_ACCESS_CODES send through Digital Ground Game Connect unless the streamer picks
// their own bot instead.
// https://docs.discord.com/developers/reference

constexpr const char *kDiscordApiUrl = "https://discord.com/api/v10";

// Whether id looks like a Discord user ID: a snowflake, 17 to 20 digits. Kept
// as a string, since it doesn't fit in a double.
bool is_discord_user_id(const std::string &id);

// The body for POST /users/@me/channels, which opens (or finds) the DM
// channel with the streamer.
// https://docs.discord.com/developers/resources/user#create-dm
std::string build_discord_open_dm(const std::string &user_id);

// The body for POST /channels/{id}/messages. Mentions are off, so nothing in
// an answer can ping anyone. content must fit Discord's 2,000 characters;
// connect_message_content's 1,800 does.
// https://docs.discord.com/developers/resources/message#create-message
std::string build_discord_message(const std::string &content);

struct DiscordReply {
	bool ok = false;
	// From opening the DM: the channel to send to.
	std::string channel_id;
	// On failure: invalid_token, dm_refused, bad_user_id, rate_limited,
	// server, bad_response or discord_error, and a message for the streamer.
	std::string error;
	std::string message;
	// For rate_limited: how many seconds Discord asks to wait.
	double retry_after = 0;
};

// Reads Discord's reply to opening the DM, given its HTTP status.
DiscordReply parse_discord_open_dm(long status, const std::string &body);

// Reads Discord's reply to sending the message.
DiscordReply parse_discord_message(long status, const std::string &body);
