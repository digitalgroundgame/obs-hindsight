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

#include "connect-api.hpp"

#include <atomic>
#include <string>

// Sends to DGG Connect over HTTPS with the streamer's Hindsight code. curl must
// already be started with curl_global_init. Set HINDSIGHT_CONNECT_URL (say,
// to http://localhost:3000) to test against a local DGG Connect.

// DMs content to the streamer. Blocks for up to about 15 seconds, or twice
// that if the first try hit a server or Discord hiccup, so call it off the
// audio and UI threads. Setting cancel stops it within a second or so.
ConnectReply connect_send_message(const std::string &code, const std::string &content, const std::atomic<bool> &cancel);

// The Test button: asks DGG Connect to send a fixed test DM, on its own thread.
enum class ConnectTestState { idle, testing, done };
struct ConnectTestStatus {
	ConnectTestState state = ConnectTestState::idle;
	ConnectReply reply; // when done
};

// Starts a test with code, unless one is running. Call from the UI thread.
void connect_test_start(const std::string &code);
ConnectTestStatus connect_test_status();
// Cancels a running test and waits for it. Call from obs_module_unload.
void connect_test_stop();
