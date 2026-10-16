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

// The request Hindsight sends to the Claude Messages API, and how it reads the
// reply. Kept free of OBS and curl so the tests can run it on its own;
// claude-client.cpp does the sending.

// A fast model keeps the streamer's wait short.
constexpr const char *kClaudeModel = "claude-haiku-5-5";
constexpr const char *kClaudeMessagesUrl = "https://api.anthropic.com/v1/messages";
constexpr const char *kClaudeApiVersion = "2023-06-01";

// How hard Claude thinks before answering, with adaptive thinking on.
// "medium" is Haiku's default; higher means better answers and a longer wait.
constexpr const char *kClaudeEffort = "medium";
// The most tokens Claude may write, thinking included. An answer is at most a
// few hundred; the rest is room to think.
constexpr int kClaudeMaxTokens = 4096;

// The longest answer Claude is asked for. The DGG server cuts anything
// longer, so this keeps answers whole.
constexpr size_t kMaxAnswerCharacters = 1700;

// The JSON body for POST /v1/messages: the transcript of the last few seconds
// of the streamer's mic, and a tool Claude is told to call with the question
// it found and its answer. Claude thinks first (adaptive thinking).
std::string build_claude_request(const std::string &transcript);

// The key is not scoped to a workspace, so the API wants an
// anthropic-workspace-id header. The streamer needs a workspace key instead.
// https://platform.claude.com/docs/en/manage-claude/authentication
inline constexpr const char *kWorkspaceKeyError = "workspace_key";

struct ClaudeAnswer {
	enum class Outcome {
		// answer is set, and question too unless Claude replied in plain
		// text instead of calling the tool
		answered,
		no_question, // Claude found no clear question; send nothing
		failed,      // see error_type and error_message
	};
	Outcome outcome = Outcome::failed;
	std::string question;
	std::string answer;

	// For a failed request: the API's error type (authentication_error,
	// rate_limit_error, overloaded_error, ...), kWorkspaceKeyError, or
	// "bad_response" when the reply could not be read, and the API's message
	// if it sent one.
	std::string error_type;
	std::string error_message;

	// Tokens the request was billed for, from the reply's usage (0 if absent).
	// output_tokens includes Claude's thinking.
	long long input_tokens = 0;
	long long output_tokens = 0;
};

// What a request to kClaudeModel cost in US dollars, at its list prices for
// prompts under 100,000 tokens (Hindsight's are far shorter):
// https://platform.claude.com/docs/en/about-claude/pricing
// Update these along with kClaudeModel.
constexpr double kClaudeInputDollarsPerMillion = 0.10;
constexpr double kClaudeOutputDollarsPerMillion = 0.50;
double claude_cost_dollars(long long input_tokens, long long output_tokens);

// Cuts text longer than max_characters (Unicode code points) to fit,
// ending it with "…". Never splits a UTF-8 character.
std::string shorten_text(const std::string &text, size_t max_characters);

// Reads the API's reply to build_claude_request, given its HTTP status. An
// answer longer than kMaxAnswerCharacters is shortened to fit.
ClaudeAnswer parse_claude_response(long status, const std::string &body);
