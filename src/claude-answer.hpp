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

// The most web searches Claude may run for one question. Each search adds a
// second or two to the streamer's wait, and simple factual questions need one
// to three.
// https://platform.claude.com/docs/en/agents-and-tools/tool-use/web-search-tool#max-uses
constexpr int kClaudeMaxSearches = 3;

// The longest answer Claude is asked for. The Digital Ground Game server cuts
// anything longer, so this keeps answers whole.
constexpr size_t kMaxAnswerCharacters = 1700;

// The JSON body for POST /v1/messages: the transcript of the last few seconds
// of the streamer's mic, their local date and time (from local_time_text) so
// Claude knows what "today" and "right now" mean, the web search tool, and a
// tool Claude is told to call with the question it found and its answer.
// Claude thinks first (adaptive thinking).
//
// paused_content resumes a turn the API paused (ClaudeAnswer::paused_content):
// it goes back as the assistant's message, unchanged, and Claude carries on.
// max_uses applies per request, so a resumed turn passes the searches it has
// left as max_searches.
std::string build_claude_request(const std::string &transcript, const std::string &now,
				 const std::string &paused_content = {}, int max_searches = kClaudeMaxSearches);

// The key is not scoped to a workspace, so the API wants an
// anthropic-workspace-id header. The streamer needs a workspace key instead.
// https://platform.claude.com/docs/en/manage-claude/authentication
inline constexpr const char *kWorkspaceKeyError = "workspace_key";

// An admin turned web search off for the key's Claude organization, so the
// API refuses any request that offers it. They can turn it back on in the
// Claude Console.
// https://platform.claude.com/docs/en/agents-and-tools/tool-use/web-search-tool#how-to-use-web-search
inline constexpr const char *kWebSearchDisabledError = "web_search_disabled";

struct ClaudeAnswer {
	enum class Outcome {
		answered,    // answer is set; question too, unless Claude answered in plain text
		no_question, // no clear question, or a plain-text reply; send nothing
		paused,      // the API paused a long search turn; resume with paused_content
		failed,      // see error_type and error_message
	};
	Outcome outcome = Outcome::failed;
	std::string question;
	std::string answer;

	// For a paused turn: the reply's content array, as JSON, to send back
	// through build_claude_request.
	std::string paused_content;

	// For a failed request: the API's error type (authentication_error,
	// rate_limit_error, overloaded_error, ...), kWorkspaceKeyError,
	// kWebSearchDisabledError, or
	// "bad_response" when the reply could not be read, and the API's message
	// if it sent one.
	std::string error_type;
	std::string error_message;

	// Tokens the request was billed for, from the reply's usage (0 if absent).
	// output_tokens includes Claude's thinking.
	long long input_tokens = 0;
	long long output_tokens = 0;
	// Web searches the request was billed for. Search results Claude read are
	// already counted in input_tokens.
	long long web_searches = 0;
};

// What a request to kClaudeModel cost in US dollars, at its list prices for
// prompts under 100,000 tokens (Hindsight's are far shorter, search results
// included), plus $10 per 1,000 web searches:
// https://platform.claude.com/docs/en/about-claude/pricing
// https://platform.claude.com/docs/en/agents-and-tools/tool-use/web-search-tool#usage-and-pricing
// Update these along with kClaudeModel.
constexpr double kClaudeInputDollarsPerMillion = 0.10;
constexpr double kClaudeOutputDollarsPerMillion = 0.50;
constexpr double kClaudeDollarsPerSearch = 0.01;
double claude_cost_dollars(long long input_tokens, long long output_tokens, long long web_searches);

// Cuts text longer than max_characters (Unicode code points) to fit,
// ending it with "…". Never splits a UTF-8 character.
std::string shorten_text(const std::string &text, size_t max_characters);

// Reads the API's reply to build_claude_request, given its HTTP status. An
// answer longer than kMaxAnswerCharacters is shortened to fit.
ClaudeAnswer parse_claude_response(long status, const std::string &body);
