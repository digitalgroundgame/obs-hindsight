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

// The request Hindsight sends to the Claude Messages API, and how it reads the
// reply. Kept free of OBS and curl so the tests can run it on its own;
// claude-client.cpp does the sending.

// A fast model keeps the streamer's wait short.
constexpr const char *kClaudeModel = "claude-haiku-5-5";
constexpr const char *kClaudeMessagesUrl = "https://api.anthropic.com/v1/messages";
constexpr const char *kClaudeApiVersion = "2023-06-01";

// The longest answer Claude is asked for. The DGG server cuts anything
// longer, so this keeps answers whole.
constexpr int kMaxAnswerCharacters = 1700;

// The JSON body for POST /v1/messages: the transcript of the last few seconds
// of the streamer's mic, and a tool Claude must call with the question it
// found and its answer.
std::string build_claude_request(const std::string &transcript);

struct ClaudeAnswer {
	enum class Outcome {
		answered,    // question and answer are set
		no_question, // Claude found no clear question; send nothing
		failed,      // see error_type and error_message
	};
	Outcome outcome = Outcome::failed;
	std::string question;
	std::string answer;

	// For a failed request: the API's error type (authentication_error,
	// rate_limit_error, overloaded_error, ...) or "bad_response" when the
	// reply could not be read, and the API's message if it sent one.
	std::string error_type;
	std::string error_message;
};

// Reads the API's reply to build_claude_request, given its HTTP status.
ClaudeAnswer parse_claude_response(long status, const std::string &body);
