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
#include "discord-api.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <exception>
#include <string>

using nlohmann::json;

namespace {

void checks_user_ids()
{
	CHECK(is_discord_user_id("80351110224678912"));
	CHECK(is_discord_user_id("12345678901234567890"));
	CHECK(!is_discord_user_id(""));
	CHECK(!is_discord_user_id("1234"));
	CHECK(!is_discord_user_id("123456789012345678901"));
	CHECK(!is_discord_user_id("8035111022467891a"));
	CHECK(!is_discord_user_id("tadjh#1234"));
}

void builds_the_bodies()
{
	// The ID stays a string: as a number it would lose digits.
	const json open = json::parse(build_discord_open_dm("80351110224678912"));
	CHECK(open["recipient_id"] == "80351110224678912");

	const json message = json::parse(build_discord_message("**Q?**\nA @everyone"));
	CHECK(message["content"] == "**Q?**\nA @everyone");
	CHECK(message["allowed_mentions"]["parse"].is_array());
	CHECK(message["allowed_mentions"]["parse"].empty());

	// Bad UTF-8 is replaced rather than thrown on.
	CHECK(json::parse(build_discord_message(std::string("caf\xc3", 4))).is_object());
}

void reads_the_dm_channel()
{
	const DiscordReply reply = parse_discord_open_dm(200, R"({"id":"319674150115610528","type":1})");
	CHECK(reply.ok);
	CHECK(reply.channel_id == "319674150115610528");

	CHECK(parse_discord_open_dm(200, "{}").error == "bad_response");
	CHECK(parse_discord_open_dm(200, "not json").error == "bad_response");
	CHECK(parse_discord_message(200, R"({"id":"1"})").ok);
}

void explains_errors()
{
	CHECK(parse_discord_open_dm(401, R"({"code":0,"message":"401: Unauthorized"})").error == "invalid_token");
	CHECK(parse_discord_message(403, R"({"code":50007,"message":"Cannot send messages to this user"})").error ==
	      "dm_refused");
	CHECK(parse_discord_open_dm(400, R"({"code":50035,"message":"Invalid Form Body"})").error == "bad_user_id");
	CHECK(parse_discord_open_dm(404, R"({"code":10013,"message":"Unknown User"})").error == "bad_user_id");

	const DiscordReply limited = parse_discord_message(
		429, R"({"message":"You are being rate limited.","retry_after":1.5,"global":false})");
	CHECK(limited.error == "rate_limited");
	CHECK(limited.retry_after == 1.5);

	CHECK(parse_discord_message(502, "<html>").error == "server");
	const DiscordReply other = parse_discord_message(403, R"({"code":50001,"message":"Missing Access"})");
	CHECK(other.error == "discord_error");
	CHECK(other.message.find("Missing Access") != std::string::npos);
	CHECK(!other.ok);
}

} // namespace

int main()
{
	try {
		checks_user_ids();
		builds_the_bodies();
		reads_the_dm_channel();
		explains_errors();
	} catch (const std::exception &e) {
		std::printf("threw: %s\n", e.what());
		return 1;
	}
	return check::finish();
}
