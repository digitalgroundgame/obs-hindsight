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
#include "claude-app.hpp"

#include <string>

namespace {

void destinations_round_trip()
{
	CHECK(answer_destination_from_id(answer_destination_id(AnswerDestination::discord)) ==
	      AnswerDestination::discord);
	CHECK(answer_destination_from_id(answer_destination_id(AnswerDestination::claude_app)) ==
	      AnswerDestination::claude_app);
	// Anything else, a missing setting included, is Discord.
	CHECK(answer_destination_from_id("") == AnswerDestination::discord);
	CHECK(answer_destination_from_id("slack") == AnswerDestination::discord);
}

void encodes_for_a_query_value()
{
	CHECK(percent_encode("Az09-._~") == "Az09-._~");
	CHECK(percent_encode("a b&c=d?e#f/g+h") == "a%20b%26c%3Dd%3Fe%23f%2Fg%2Bh");
	CHECK(percent_encode("line\nnext") == "line%0Anext");
	CHECK(percent_encode("caf\xC3\xA9") == "caf%C3%A9"); // é, byte by byte
	CHECK(percent_encode("\"%'") == "%22%25%27");
	CHECK(percent_encode("").empty());
}

void link_opens_a_new_chat_with_the_transcript()
{
	const std::string link = claude_app_link("so what year did the wall fall?");
	CHECK(link.rfind("claude://claude.ai/new?q=", 0) == 0);
	CHECK(link.find("what%20year%20did%20the%20wall%20fall%3F") != std::string::npos);
	// Nothing in the value can end it or start another parameter.
	const std::string value = link.substr(std::string(kClaudeAppNewChat).size());
	CHECK(value.find_first_of("&#= \n") == std::string::npos);

	const std::string prompt = claude_app_prompt("so what year did the wall fall?");
	CHECK(prompt.find("most recent question") != std::string::npos);
	CHECK(prompt.size() >= 31 && prompt.substr(prompt.size() - 31) == "so what year did the wall fall?");
}

void long_transcripts_keep_their_end()
{
	// The newest speech, with the question, is at the end.
	std::string transcript(kClaudeAppMaxTranscript + 500, 'a');
	transcript += " the question?";
	const std::string prompt = claude_app_prompt(transcript);
	CHECK(prompt.find("\xE2\x80\xA6") != std::string::npos); // …
	CHECK(prompt.substr(prompt.size() - 14) == " the question?");
	CHECK(prompt.size() < kClaudeAppMaxTranscript + 400);

	// A cut never lands inside a UTF-8 character.
	std::string accented;
	for (size_t i = 0; i < kClaudeAppMaxTranscript; ++i)
		accented += "\xC3\xA9";
	const std::string cut = claude_app_prompt(accented);
	const size_t after_mark = cut.find("\xE2\x80\xA6") + 3; // past …
	CHECK(static_cast<unsigned char>(cut[after_mark]) == 0xC3);
}

} // namespace

int main()
{
	destinations_round_trip();
	encodes_for_a_query_value();
	link_opens_a_new_chat_with_the_transcript();
	long_transcripts_keep_their_end();
	return check::finish();
}
