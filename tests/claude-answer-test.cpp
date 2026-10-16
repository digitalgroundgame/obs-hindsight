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

#include "check.hpp"
#include "claude-answer.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <exception>
#include <string>

using nlohmann::json;

namespace {

std::string tool_reply(const std::string &question, const std::string &answer)
{
	const json reply = {
		{"type", "message"},
		{"stop_reason", "tool_use"},
		{"content", json::array({{{"type", "text"}, {"text", "Sure."}},
					 {{"type", "tool_use"},
					  {"id", "toolu_1"},
					  {"name", "send_answer"},
					  {"input", {{"question", question}, {"answer", answer}}}}})},
	};
	return reply.dump();
}

void request_thinks_offers_the_tool_and_carries_the_transcript()
{
	const json request = json::parse(build_claude_request("what year did \"the wall\" fall\n"));

	CHECK(request["model"] == kClaudeModel);
	CHECK(request["max_tokens"] == kClaudeMaxTokens);
	CHECK(request["thinking"]["type"] == "adaptive");
	CHECK(request["output_config"]["effort"] == kClaudeEffort);
	// A forced tool call would skip thinking, so Claude chooses to call it.
	CHECK(request["tool_choice"]["type"] == "auto");
	CHECK(request["tools"][0]["name"] == "send_answer");
	CHECK(request["tools"][0]["strict"] == true);
	CHECK(request["tools"][0]["input_schema"]["additionalProperties"] == false);
	CHECK(request["messages"].size() == 1);
	CHECK(request["messages"][0]["role"] == "user");
	const std::string content = request["messages"][0]["content"];
	CHECK(content.find("what year did \"the wall\" fall") != std::string::npos);
	CHECK(request["system"].get<std::string>().find("1,700") != std::string::npos);
}

void request_survives_bad_utf8()
{
	// Transcription can split a character; the request must still be valid JSON.
	const std::string body = build_claude_request(std::string("caf\xc3", 4));
	CHECK(!json::parse(body, nullptr, false).is_discarded());
}

void reads_the_question_and_answer()
{
	const ClaudeAnswer answer = parse_claude_response(200, tool_reply(" What year? ", "1989, on November 9.\n"));
	CHECK(answer.outcome == ClaudeAnswer::Outcome::answered);
	CHECK(answer.question == "What year?");
	CHECK(answer.answer == "1989, on November 9.");
}

void skips_thinking_blocks()
{
	const ClaudeAnswer answer = parse_claude_response(
		200,
		R"({"stop_reason":"tool_use","content":[{"type":"thinking","thinking":"","signature":"abc"},{"type":"tool_use","name":"send_answer","input":{"question":"Q?","answer":"A."}}]})");
	CHECK(answer.outcome == ClaudeAnswer::Outcome::answered);
	CHECK(answer.question == "Q?");
	CHECK(answer.answer == "A.");
}

void plain_text_reply_is_the_answer()
{
	// Claude answered without calling the tool: the text is sent, with no question.
	const ClaudeAnswer answer = parse_claude_response(
		200,
		R"({"stop_reason":"end_turn","content":[{"type":"thinking","thinking":"","signature":"abc"},{"type":"text","text":" 1989, "},{"type":"text","text":"on November 9.\n"}],"usage":{"input_tokens":10,"output_tokens":20}})");
	CHECK(answer.outcome == ClaudeAnswer::Outcome::answered);
	CHECK(answer.question.empty());
	CHECK(answer.answer == "1989, on November 9.");
	CHECK(answer.output_tokens == 20);

	const std::string long_text(kMaxAnswerCharacters + 10, 'a');
	const ClaudeAnswer long_answer = parse_claude_response(
		200,
		json({{"stop_reason", "end_turn"}, {"content", {{{"type", "text"}, {"text", long_text}}}}}).dump());
	CHECK(long_answer.answer.size() == kMaxAnswerCharacters - 1 + 3);

	// Only thinking and no text has nothing to send.
	const ClaudeAnswer silent = parse_claude_response(
		200, R"({"stop_reason":"end_turn","content":[{"type":"thinking","thinking":"","signature":"abc"}]})");
	CHECK(silent.outcome == ClaudeAnswer::Outcome::failed);
	CHECK(silent.error_type == "bad_response");
}

void reads_the_usage_and_cost()
{
	const ClaudeAnswer answer = parse_claude_response(
		200,
		R"({"content":[{"type":"tool_use","name":"send_answer","input":{"question":"Q?","answer":"A."}}],"usage":{"input_tokens":812,"output_tokens":95}})");
	CHECK(answer.input_tokens == 812);
	CHECK(answer.output_tokens == 95);
	// 812 at $0.10 and 95 at $0.50 per million tokens.
	CHECK(std::fabs(claude_cost_dollars(812, 95) - 0.0001287) < 1e-12);
	CHECK(claude_cost_dollars(0, 0) == 0.0);

	// No usage in the reply reads as zero rather than failing.
	const ClaudeAnswer no_usage = parse_claude_response(200, tool_reply("Q?", "A."));
	CHECK(no_usage.outcome == ClaudeAnswer::Outcome::answered);
	CHECK(no_usage.input_tokens == 0);
	CHECK(no_usage.output_tokens == 0);
}

void empty_fields_mean_no_question()
{
	CHECK(parse_claude_response(200, tool_reply("", "")).outcome == ClaudeAnswer::Outcome::no_question);
	CHECK(parse_claude_response(200, tool_reply("  ", "\n")).outcome == ClaudeAnswer::Outcome::no_question);

	// Half an answer has nothing worth sending either.
	const ClaudeAnswer half = parse_claude_response(200, tool_reply("What year?", ""));
	CHECK(half.outcome == ClaudeAnswer::Outcome::no_question);
	CHECK(half.question.empty());
}

void reads_api_errors()
{
	const ClaudeAnswer bad_key = parse_claude_response(
		401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})");
	CHECK(bad_key.outcome == ClaudeAnswer::Outcome::failed);
	CHECK(bad_key.error_type == "authentication_error");
	CHECK(bad_key.error_message == "invalid x-api-key");

	const ClaudeAnswer org_key = parse_claude_response(
		400,
		R"({"type":"error","error":{"type":"invalid_request_error","message":"This API key is not scoped to a workspace, so this request must include the anthropic-workspace-id header with the ID of the workspace to use."}})");
	CHECK(org_key.error_type == kWorkspaceKeyError);

	const ClaudeAnswer other_400 = parse_claude_response(
		400, R"({"type":"error","error":{"type":"invalid_request_error","message":"max_tokens: too large"}})");
	CHECK(other_400.error_type == "invalid_request_error");

	const ClaudeAnswer busy = parse_claude_response(529, "<html>overloaded</html>");
	CHECK(busy.outcome == ClaudeAnswer::Outcome::failed);
	CHECK(busy.error_type == "http_529");
}

void rejects_replies_it_cannot_read()
{
	CHECK(parse_claude_response(200, "not json").error_type == "bad_response");
	CHECK(parse_claude_response(200, "[]").error_type == "bad_response");
	CHECK(parse_claude_response(200, R"({"content":"text"})").error_type == "bad_response");

	// Claude ran out of tokens before calling the tool; half a text answer is
	// not sent, but what it cost still is reported.
	const ClaudeAnswer cut_off = parse_claude_response(
		200,
		R"({"stop_reason":"max_tokens","content":[{"type":"text","text":"Let me think"}],"usage":{"input_tokens":5,"output_tokens":4096}})");
	CHECK(cut_off.outcome == ClaudeAnswer::Outcome::failed);
	CHECK(cut_off.error_type == "bad_response");
	CHECK(cut_off.error_message == "stop_reason max_tokens");
	CHECK(cut_off.output_tokens == 4096);

	const ClaudeAnswer refused = parse_claude_response(
		200, R"({"stop_reason":"refusal","content":[{"type":"text","text":"I can't help with that."}]})");
	CHECK(refused.outcome == ClaudeAnswer::Outcome::failed);

	// Wrong types inside the tool input are treated as empty, not thrown on.
	const ClaudeAnswer odd = parse_claude_response(
		200, R"({"content":[{"type":"tool_use","name":"send_answer","input":{"question":7,"answer":null}}]})");
	CHECK(odd.outcome == ClaudeAnswer::Outcome::no_question);
}

void shortens_long_text()
{
	CHECK(shorten_text("hello", 5) == "hello");
	CHECK(shorten_text("hello!", 5) == "hell\xE2\x80\xA6");
	// "é" is two bytes but one character, and is never split.
	CHECK(shorten_text("\xC3\xA9\xC3\xA9\xC3\xA9", 3) == "\xC3\xA9\xC3\xA9\xC3\xA9");
	CHECK(shorten_text("\xC3\xA9\xC3\xA9\xC3\xA9", 2) == "\xC3\xA9\xE2\x80\xA6");
}

void long_answers_are_shortened()
{
	const std::string answer(kMaxAnswerCharacters + 50, 'a');
	const ClaudeAnswer result = parse_claude_response(200, tool_reply("Why?", answer));
	CHECK(result.outcome == ClaudeAnswer::Outcome::answered);
	// kMaxAnswerCharacters - 1 letters and a three-byte ellipsis.
	CHECK(result.answer.size() == kMaxAnswerCharacters - 1 + 3);
	CHECK(result.answer.compare(result.answer.size() - 3, 3, "\xE2\x80\xA6") == 0);
}

} // namespace

int main()
{
	// The tests build JSON with nlohmann/json, which throws on misuse.
	try {
		request_thinks_offers_the_tool_and_carries_the_transcript();
		request_survives_bad_utf8();
		reads_the_question_and_answer();
		skips_thinking_blocks();
		plain_text_reply_is_the_answer();
		reads_the_usage_and_cost();
		empty_fields_mean_no_question();
		reads_api_errors();
		rejects_replies_it_cannot_read();
		shortens_long_text();
		long_answers_are_shortened();
	} catch (const std::exception &e) {
		std::printf("threw: %s\n", e.what());
		return 1;
	}
	return check::finish();
}
