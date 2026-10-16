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

#include "claude-answer.hpp"

#include <atomic>
#include <string>

// Sends the transcript to Claude with the streamer's own API key and waits for
// the answer. Blocks for up to a minute, so call it off the audio and UI
// threads. Setting cancel (say, while OBS closes) stops it within a second or
// so, returning a failed answer with error_type "cancelled". curl must already
// be started with curl_global_init.
ClaudeAnswer ask_claude(const std::string &transcript, const std::string &api_key, const std::atomic<bool> &cancel);
