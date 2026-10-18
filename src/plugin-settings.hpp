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
#include "claude-app.hpp"

// Hindsight's preferences that are not secrets, kept in settings.json in the
// plugin's config folder (separate from the keys file). Safe to call from any
// thread: the settings window saves them on OBS's UI thread while the answer
// pipeline reads them on its own.

// Whether to ask GitHub for a newer release when OBS starts. On unless the
// streamer turned it off; a missing or unreadable file counts as on.
bool plugin_settings_check_for_updates();

// Saves the choice. Returns false if the file could not be written.
bool plugin_settings_set_check_for_updates(bool on);

// The Claude model and web search choice. The default model with web search
// on unless the streamer changed them; a missing or unreadable file counts as
// the defaults.
ClaudeOptions plugin_settings_claude_options();

// Saves the choice. Returns false if the file could not be written.
bool plugin_settings_set_claude_options(const ClaudeOptions &options);

// Where the hotkey sends what it heard. Discord unless the streamer picked
// the Claude app; a missing or unreadable file counts as Discord.
AnswerDestination plugin_settings_destination();

// Saves the choice. Returns false if the file could not be written.
bool plugin_settings_set_destination(AnswerDestination destination);
