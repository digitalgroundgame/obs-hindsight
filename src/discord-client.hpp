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

#include "discord-api.hpp"

#include <atomic>
#include <string>

// DMs content to user_id from the streamer's own bot: opens the DM channel,
// then sends the message, waiting out one rate limit if Discord asks. Blocks
// for up to about half a minute, so call it off the audio and UI threads.
// Setting cancel stops it within a second or so, returning error
// "cancelled". curl must already be started with curl_global_init.
DiscordReply discord_send_dm(const std::string &bot_token, const std::string &user_id, const std::string &content,
			     const std::atomic<bool> &cancel);
