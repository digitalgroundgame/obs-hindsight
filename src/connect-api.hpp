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

#include <cstddef>
#include <string>

// What Hindsight sends to DGG Connect, which DMs the answer to the streamer
// with the DGG Discord bot, and how it reads the reply. Kept free of OBS and
// curl so the tests can run it on its own; connect-client.cpp does the
// sending. The API is in the digitalgroundgame/connect README.

constexpr const char *kConnectUrl = "https://connect.digitalgroundgame.org";
constexpr const char *kConnectMessagesPath = "/api/v1/messages";
constexpr const char *kConnectTestPath = "/api/v1/test";

// The longest message DGG Connect takes, counted as JavaScript counts string
// length (UTF-16 code units). It adds its own footer within Discord's 2,000.
constexpr size_t kConnectMaxContent = 1800;

// The DM: the question in bold, then the answer, shortened to fit
// kConnectMaxContent. A missing question leaves just the answer.
std::string connect_message_content(const std::string &question, const std::string &answer);

// The JSON body for POST /api/v1/messages. id names this answer, so a retry
// with the same id is not sent twice.
std::string build_connect_message(const std::string &content, const std::string &id);

// A new id for build_connect_message: 32 random hex digits.
std::string new_connect_message_id();

// How long text is in UTF-16 code units, as JavaScript counts it.
size_t utf16_length(const std::string &text);

struct ConnectReply {
	bool ok = false;
	// On success: the streamer's Discord name and the project, as DGG Connect
	// knows them.
	std::string discord_name;
	std::string project;
	// On failure: DGG Connect's error code (invalid_code, dm_closed,
	// rate_limited, ...), "not_found" when the server has no such API, or
	// "bad_response" when the reply could not be read; and a message to show
	// the streamer.
	std::string error;
	std::string message;
	// Whether trying once more, with the same id, might work.
	bool retry = false;
};

// Reads DGG Connect's reply, given its HTTP status.
ConnectReply parse_connect_response(long status, const std::string &body);
