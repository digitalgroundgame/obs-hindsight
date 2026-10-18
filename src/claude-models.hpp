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

// The Claude models a streamer can pick in Hindsight settings, with their list
// prices for prompts under 100,000 tokens (Hindsight's are far shorter, search
// results included):
// https://platform.claude.com/docs/en/about-claude/pricing
// Kept free of OBS so the tests can run it on its own.

struct ClaudeModel {
	const char *id;    // what the Messages API calls it
	const char *label; // what the settings window shows
	double input_dollars_per_million;
	double output_dollars_per_million;
};

// Fastest first. The first is the default: a fast model keeps the streamer's
// wait short.
inline constexpr ClaudeModel kClaudeModels[] = {
	{"claude-haiku-5-5", "Claude Haiku 5.5 (fastest, cheapest)", 0.10, 0.50},
	{"claude-sonnet-5-5", "Claude Sonnet 5.5 (smarter, slower)", 2.00, 10.00},
	{"claude-opus-5-5", "Claude Opus 5.5 (smartest, slowest)", 4.00, 20.00},
};
inline constexpr size_t kClaudeModelCount = sizeof(kClaudeModels) / sizeof(kClaudeModels[0]);
inline constexpr const ClaudeModel &kDefaultClaudeModel = kClaudeModels[0];

// The model with this id, or the default if Hindsight no longer offers it
// (say, a settings file saved by an older version).
const ClaudeModel &claude_model(const std::string &id);
