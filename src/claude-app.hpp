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

// Where Hindsight sends what the hotkey heard, and the link that opens a new
// chat in the Claude desktop app with it. Kept free of OBS so the tests can
// run it on its own; claude-app-open.cpp does the opening.

enum class AnswerDestination {
	// Hindsight asks Claude with the streamer's API key and DMs the answer
	// (through DGG Connect, in builds with HINDSIGHT_ACCESS_CODES).
	discord,
	// Hindsight opens a new chat in the Claude desktop app with the
	// transcript in the message box. The streamer sends it there, so their
	// Claude plan answers it and no API key is needed.
	claude_app,
};

// How settings.json names each destination.
const char *answer_destination_id(AnswerDestination destination);

// The destination with this id, or discord for anything else (say, a
// settings file saved by an older version).
AnswerDestination answer_destination_from_id(const std::string &id);

// The Claude desktop app's link for a new chat with q in the message box. It
// is not sent: the streamer reviews it and presses send.
// https://support.claude.com/en/articles/14729294-open-claude-desktop-with-a-link
constexpr const char *kClaudeAppNewChat = "claude://claude.ai/new?q=";

// The app cuts q at about 14,000 characters. A transcript is far shorter;
// this keeps the cut at the end, where the newest question is, from ever
// being the app's.
constexpr size_t kClaudeAppMaxTranscript = 12000;

// What goes in the message box: a line saying what the transcript is, then
// the transcript.
std::string claude_app_prompt(const std::string &transcript);

// kClaudeAppNewChat followed by claude_app_prompt(transcript), percent-encoded.
std::string claude_app_link(const std::string &transcript);

// Percent-encodes text for a URL query value: everything but RFC 3986's
// unreserved characters (letters, digits, - . _ ~), byte by byte, so UTF-8
// stays UTF-8.
std::string percent_encode(const std::string &text);
