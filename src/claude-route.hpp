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

#include "claude-models.hpp"

#include <string>

// Picking the model automatically: before answering, Hindsight asks the
// fastest model whether the question needs a smarter one. Most questions on
// stream are quick facts, so most stay on the fast model; the check adds
// about a second and a small fraction of a cent. Kept free of OBS and curl
// so the tests can run it on its own; claude-client.cpp does the sending.

// The model that decides, and the two it picks between. Opus is left out:
// its answers take too long for a live stream.
inline constexpr const char *kRouteModel = "claude-haiku-5-5";
inline constexpr const char *kRouteFastModel = "claude-haiku-5-5";
inline constexpr const char *kRouteSmartModel = "claude-sonnet-5-5";

// The JSON body for POST /v1/messages: the transcript, and a route tool
// Claude must call with "fast" or "smart". A forced tool call skips
// thinking, which keeps the check quick.
std::string build_route_request(const std::string &transcript);

struct ClaudeRoute {
	bool ok = false;
	// The model to answer with: kRouteFastModel or kRouteSmartModel.
	std::string model;
	// For a failed request: the API's error type, or "bad_response".
	std::string error_type;
	long long input_tokens = 0;
	long long output_tokens = 0;
};

// Reads the API's reply to build_route_request, given its HTTP status.
ClaudeRoute parse_route_response(long status, const std::string &body);
