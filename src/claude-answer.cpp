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

using nlohmann::json;

namespace {

constexpr const char *kToolName = "send_answer";

const char *const kSystemPrompt =
	"You help a live streamer. You get an automatic transcript of the last few seconds of their microphone. "
	"It may start mid-sentence and may have transcription mistakes.\n\n"
	"Find the most recent question the streamer asked, whether to chat, to a guest or out loud to themselves, "
	"and answer it. The answer goes to the streamer in a Discord message, so:\n"
	"- Answer directly and briefly, in plain text with no Markdown.\n"
	"- Keep the answer under 1,700 characters.\n"
	"- If you are not sure of a fact, say so rather than guess.\n\n"
	"If there is no clear question, leave both fields empty. Never invent a question.\n\n"
	"Always reply by calling the send_answer tool.";

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

ClaudeAnswer failure(std::string type, std::string message = {})
{
	ClaudeAnswer result;
	result.error_type = std::move(type);
	result.error_message = std::move(message);
	return result;
}

} // namespace

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

std::string build_claude_request(const std::string &transcript)
{
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
	};

	const json request = {
		{"model", kClaudeModel},
		{"max_tokens", 1024},
		{"system", kSystemPrompt},
		{"tools", json::array({{
				  {"name", kToolName},
				  {"description", "Send the question you found and your answer to the streamer."},
				  {"input_schema", input_schema},
			  }})},
		{"tool_choice", {{"type", "tool"}, {"name", kToolName}}},
		{"messages", json::array({{
				     {"role", "user"},
				     {"content", "<transcript>\n" + transcript + "\n</transcript>"},
			     }})},
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

	for (const json &block : *content) {
		if (!block.is_object() || string_at(block, "type") != "tool_use" ||
		    string_at(block, "name") != kToolName)
			continue;
		const auto input = block.find("input");
		if (input == block.end() || !input->is_object())
			return failure("bad_response");

		ClaudeAnswer result;
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
	// Claude stopped before calling the tool, say at max_tokens.
	return failure("bad_response", "stop_reason " + string_at(reply, "stop_reason"));
}
