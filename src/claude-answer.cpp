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

#include "claude-answer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

using nlohmann::json;

namespace {

constexpr const char *kToolName = "send_answer";
constexpr const char *kSearchToolType = "web_search_20250305";

const char *const kSystemPrompt =
	"You help a live streamer. You get an automatic transcript of the last few seconds of their microphone. "
	"It may start mid-sentence and may have transcription mistakes.\n\n"
	"Find the most recent question the streamer asked, whether to chat, to a guest or out loud to themselves, "
	"and answer it. The answer goes to the streamer in a Discord message, so:\n"
	"- Answer directly and briefly, in plain text with no Markdown.\n"
	"- Keep the answer under 1,700 characters.\n"
	"- If you are not sure of a fact, say so rather than guess.\n\n"
	"Search the web first when the answer depends on current or recent information, such as news, ongoing "
	"events, scores, prices or who holds a post now, and answer from what you find. When you searched, name "
	"your sources briefly at the end of the answer (for example \"Sources: Reuters, AP\"). Answer stable "
	"knowledge without searching.\n\n"
	"If there is no clear question, leave both fields empty. Never invent a question.\n\n"
	"Always reply by calling the send_answer tool, even when there is no question. Never answer in plain text.";

std::string trimmed(const std::string &text)
{
	const char *space = " \t\r\n";
	const size_t first = text.find_first_not_of(space);
	if (first == std::string::npos)
		return {};
	const size_t last = text.find_last_not_of(space);
	return text.substr(first, last - first + 1);
}

std::string string_at(const json &object, const char *key)
{
	const auto it = object.find(key);
	if (it == object.end() || !it->is_string())
		return {};
	return it->get<std::string>();
}

long long count_at(const json &object, const char *key)
{
	const auto it = object.find(key);
	if (it == object.end() || !it->is_number_integer())
		return 0;
	return it->get<long long>();
}

bool is_web_search_disabled(std::string message)
{
	std::transform(message.begin(), message.end(), message.begin(),
		       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return message.find("not enabled") != std::string::npos &&
	       (message.find("web search") != std::string::npos || message.find("web_search") != std::string::npos);
}

ClaudeAnswer failure(std::string type, std::string message = {})
{
	ClaudeAnswer result;
	result.error_type = std::move(type);
	result.error_message = std::move(message);
	return result;
}

} // namespace

double claude_cost_dollars(long long input_tokens, long long output_tokens, long long web_searches)
{
	return (static_cast<double>(input_tokens) * kClaudeInputDollarsPerMillion +
		static_cast<double>(output_tokens) * kClaudeOutputDollarsPerMillion) /
		       1e6 +
	       static_cast<double>(web_searches) * kClaudeDollarsPerSearch;
}

std::string shorten_text(const std::string &text, size_t max_characters)
{
	if (max_characters == 0)
		return {};
	// Counts code points by their first byte; continuation bytes are 10xxxxxx.
	size_t characters = 0;
	size_t cut = 0; // where the kept part ends, leaving room for the ellipsis
	for (size_t i = 0; i < text.size(); i++) {
		if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80)
			continue;
		if (characters == max_characters - 1)
			cut = i;
		if (++characters > max_characters)
			return text.substr(0, cut) + "\xE2\x80\xA6"; // …
	}
	return text;
}

std::string build_claude_request(const std::string &transcript, const std::string &today,
				 const std::string &paused_content)
{
	// strict keeps the tool's input to this exact shape now that Claude is no
	// longer forced to call it; strict needs additionalProperties false.
	const json input_schema = {
		{"type", "object"},
		{"properties",
		 {
			 {"question",
			  {{"type", "string"},
			   {"description",
			    "The most recent question the streamer asked, in their words, tidied up. Empty if there is none."}}},
			 {"answer",
			  {{"type", "string"},
			   {"description",
			    "Your answer to that question, in plain text. Empty if there is no question."}}},
		 }},
		{"required", {"question", "answer"}},
		{"additionalProperties", false},
	};

	json messages = json::array({{
		{"role", "user"},
		{"content", "Today's date: " + today + "\n\n<transcript>\n" + transcript + "\n</transcript>"},
	}});
	if (!paused_content.empty()) {
		// Sent back exactly as received, so the API can pick the turn up
		// where it paused.
		json content = json::parse(paused_content, nullptr, false);
		if (content.is_array())
			messages.push_back({{"role", "assistant"}, {"content", std::move(content)}});
	}

	const json request = {
		{"model", kClaudeModel},
		// Thinking counts toward max_tokens, so leave room for it on top of
		// the answer.
		{"max_tokens", kClaudeMaxTokens},
		// Claude decides how long to think; medium effort keeps the wait
		// short. A forced tool call would skip thinking, so tool_choice is
		// auto and the system prompt tells Claude to call the tool.
		{"thinking", {{"type", "adaptive"}}},
		{"output_config", {{"effort", kClaudeEffort}}},
		{"system", kSystemPrompt},
		// Basic web search: Claude calls it directly. The newer versions filter
		// results by running code first, which adds to the wait.
		{"tools",
		 json::array({
			 {
				 {"type", kSearchToolType},
				 {"name", "web_search"},
				 {"max_uses", kClaudeMaxSearches},
			 },
			 {
				 {"name", kToolName},
				 {"description", "Send the question you found and your answer to the streamer."},
				 {"input_schema", input_schema},
				 {"strict", true},
			 },
		 })},
		{"tool_choice", {{"type", "auto"}}},
		{"messages", std::move(messages)},
	};
	// Replace bad UTF-8 from the transcript rather than throw.
	return request.dump(-1, ' ', false, json::error_handler_t::replace);
}

ClaudeAnswer parse_claude_response(long status, const std::string &body)
{
	const json reply = json::parse(body, nullptr, false);

	if (status != 200) {
		// Errors come back as {"type": "error", "error": {"type", "message"}}.
		if (reply.is_object()) {
			const auto error = reply.find("error");
			if (error != reply.end() && error->is_object()) {
				const std::string type = string_at(*error, "type");
				std::string message = string_at(*error, "message");
				// A key made for the whole organization, not one workspace, needs
				// an anthropic-workspace-id header. The API has no error type of
				// its own for this, so the message is the only way to tell.
				if (status == 400 && message.find("anthropic-workspace-id") != std::string::npos)
					return failure(kWorkspaceKeyError, std::move(message));
				// Likewise for web search turned off in the Console: the API
				// says web search is not enabled.
				if (status == 400 && is_web_search_disabled(message))
					return failure(kWebSearchDisabledError, std::move(message));
				if (!type.empty())
					return failure(type, std::move(message));
			}
		}
		return failure("http_" + std::to_string(status));
	}

	if (!reply.is_object())
		return failure("bad_response");
	const auto content = reply.find("content");
	if (content == reply.end() || !content->is_array())
		return failure("bad_response");

	ClaudeAnswer result;
	// usage.output_tokens includes the thinking tokens, which are billed as
	// output, so the cost covers them.
	const auto usage = reply.find("usage");
	if (usage != reply.end() && usage->is_object()) {
		result.input_tokens = count_at(*usage, "input_tokens");
		result.output_tokens = count_at(*usage, "output_tokens");
		const auto server_tools = usage->find("server_tool_use");
		if (server_tools != usage->end() && server_tools->is_object())
			result.web_searches = count_at(*server_tools, "web_search_requests");
	}

	std::string text; // any plain text Claude wrote outside the tool call
	for (const json &block : *content) {
		if (!block.is_object())
			continue;
		const std::string type = string_at(block, "type");
		if (type == "text") {
			text += string_at(block, "text");
			continue;
		}
		if (type != "tool_use" || string_at(block, "name") != kToolName)
			continue; // thinking, web search calls and their results, among others
		const auto input = block.find("input");
		if (input == block.end() || !input->is_object())
			return failure("bad_response");

		result.question = trimmed(string_at(*input, "question"));
		result.answer = shorten_text(trimmed(string_at(*input, "answer")), kMaxAnswerCharacters);
		// A question with no answer (or the reverse) has nothing to send.
		if (result.question.empty() || result.answer.empty()) {
			result.outcome = ClaudeAnswer::Outcome::no_question;
			result.question.clear();
			result.answer.clear();
		} else {
			result.outcome = ClaudeAnswer::Outcome::answered;
		}
		return result;
	}

	const std::string stop_reason = string_at(reply, "stop_reason");
	// The API paused a long turn of searching. Sending the content back
	// unchanged lets Claude finish it.
	// https://platform.claude.com/docs/en/agents-and-tools/tool-use/web-search-tool#pause-turn-stop-reason
	if (stop_reason == "pause_turn") {
		result.outcome = ClaudeAnswer::Outcome::paused;
		result.paused_content = content->dump(-1, ' ', false, json::error_handler_t::replace);
		return result;
	}
	// Now that the call is not forced, Claude may reply in plain text
	// instead. That is most often a remark that there is no question ("I
	// don't see a question"), which must not reach the streamer, so plain
	// text counts as no question.
	if (stop_reason == "end_turn" && !trimmed(text).empty()) {
		result.outcome = ClaudeAnswer::Outcome::no_question;
		return result;
	}
	// Claude stopped before answering, say at max_tokens or with a refusal.
	ClaudeAnswer failed = failure("bad_response", "stop_reason " + stop_reason);
	failed.input_tokens = result.input_tokens;
	failed.output_tokens = result.output_tokens;
	failed.web_searches = result.web_searches;
	return failed;
}
