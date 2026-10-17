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
#include "connect-api.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <exception>
#include <string>

namespace {

void formats_the_message()
{
	CHECK(connect_message_content("How tall is Everest?", "8,849 m.") == "**How tall is Everest?**\n8,849 m.");
	CHECK(connect_message_content("", "8,849 m.") == "8,849 m.");
}

void counts_like_javascript()
{
	CHECK(utf16_length("abc") == 3);
	CHECK(utf16_length("caf\xC3\xA9") == 4);      // é is one unit
	CHECK(utf16_length("\xE2\x80\xA6") == 1);     // … is one unit
	CHECK(utf16_length("\xF0\x9F\x98\x80") == 2); // 😀 is a surrogate pair
}

void shortens_long_messages_to_fit()
{
	const std::string long_answer(3000, 'a');
	const std::string content = connect_message_content("Q?", long_answer);
	CHECK(utf16_length(content) == kConnectMaxContent);
	CHECK(content.compare(content.size() - 3, 3, "\xE2\x80\xA6") == 0);

	// Emoji take two units each, so fewer of them fit.
	std::string emoji;
	for (int i = 0; i < 1500; i++)
		emoji += "\xF0\x9F\x98\x80";
	const std::string shortened = connect_message_content("", emoji);
	CHECK(utf16_length(shortened) <= kConnectMaxContent);
	CHECK(utf16_length(shortened) >= kConnectMaxContent - 1);
	// Whole emoji only, then the ellipsis.
	CHECK((shortened.size() - 3) % 4 == 0);

	const std::string fits(kConnectMaxContent, 'b');
	CHECK(connect_message_content("", fits) == fits);
}

void builds_the_request()
{
	const auto body = nlohmann::json::parse(build_connect_message("hi \"there\"", "abc"));
	CHECK(body["content"] == "hi \"there\"");
	CHECK(body["id"] == "abc");
	CHECK(body.size() == 2);

	// Bad UTF-8 is replaced, not thrown.
	const auto bad = nlohmann::json::parse(build_connect_message("a\xFF", "id"));
	CHECK(bad["content"] == "a\xEF\xBF\xBD");
}

void makes_fresh_ids()
{
	const std::string a = new_connect_message_id();
	const std::string b = new_connect_message_id();
	CHECK(a.size() == 32);
	CHECK(a.find_first_not_of("0123456789abcdef") == std::string::npos);
	CHECK(a != b);
}

void reads_success()
{
	const ConnectReply reply =
		parse_connect_response(200, R"({"ok":true,"project":"Hindsight","discordName":"tadjh"})");
	CHECK(reply.ok);
	CHECK(reply.discord_name == "tadjh");
	CHECK(reply.project == "Hindsight");
	CHECK(reply.error.empty());
}

void reads_errors()
{
	const ConnectReply closed = parse_connect_response(
		409, R"({"ok":false,"error":"dm_closed","message":"Turn on DMs from server members."})");
	CHECK(!closed.ok);
	CHECK(closed.error == "dm_closed");
	CHECK(closed.message == "Turn on DMs from server members.");
	CHECK(!closed.retry);

	const ConnectReply discord = parse_connect_response(
		502, R"({"ok":false,"error":"discord_unavailable","message":"Discord is down."})");
	CHECK(discord.retry);

	const ConnectReply limited = parse_connect_response(429, R"({"ok":false,"error":"rate_limited"})");
	CHECK(limited.error == "rate_limited");
	CHECK(!limited.message.empty());
	CHECK(!limited.retry);
}

void handles_odd_replies()
{
	const ConnectReply missing = parse_connect_response(404, "<html>Not Found</html>");
	CHECK(!missing.ok);
	CHECK(missing.error == "not_found");
	CHECK(!missing.retry);

	const ConnectReply garbled = parse_connect_response(500, "oops");
	CHECK(garbled.error == "bad_response");
	CHECK(garbled.retry);
	CHECK(!garbled.message.empty());

	// ok must be true, not just a 200.
	const ConnectReply odd = parse_connect_response(200, R"({"ok":false})");
	CHECK(!odd.ok);
	CHECK(odd.error == "bad_response");
	CHECK(!parse_connect_response(200, "[]").ok);
	CHECK(!parse_connect_response(200, R"({"ok":"yes"})").ok);
}

} // namespace

int main()
{
	// The tests read JSON with nlohmann/json, which throws on misuse.
	try {
		formats_the_message();
		counts_like_javascript();
		shortens_long_messages_to_fit();
		builds_the_request();
		makes_fresh_ids();
		reads_success();
		reads_errors();
		handles_odd_replies();
	} catch (const std::exception &e) {
		std::printf("threw: %s\n", e.what());
		return 1;
	}
	return check::finish();
}
