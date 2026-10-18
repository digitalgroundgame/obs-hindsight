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
#include "claude-route.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <exception>
#include <string>

using nlohmann::json;

namespace {

std::string route_reply(const std::string &pick)
{
	const json reply = {
		{"type", "message"},
		{"stop_reason", "tool_use"},
		{"content",
		 json::array(
			 {{{"type", "tool_use"}, {"id", "toolu_1"}, {"name", "route"}, {"input", {{"model", pick}}}}})},
		{"usage", {{"input_tokens", 210}, {"output_tokens", 12}}},
	};
	return reply.dump();
}

void request_forces_the_route_tool()
{
	const json request = json::parse(build_route_request("what's 17 times 23 and why"));
	CHECK(request["model"] == kRouteModel);
	CHECK(request["tool_choice"]["type"] == "tool");
	CHECK(request["tool_choice"]["name"] == "route");
	CHECK(request["tools"].size() == 1);
	CHECK(request["tools"][0]["strict"] == true);
	CHECK(request["tools"][0]["input_schema"]["properties"]["model"]["enum"] == json({"fast", "smart"}));
	CHECK(request["messages"][0]["content"].get<std::string>().find("17 times 23") != std::string::npos);
	// Forced tool calls skip thinking, so none is asked for.
	CHECK(!request.contains("thinking"));

	// Bad UTF-8 is replaced rather than thrown on.
	CHECK(json::parse(build_route_request(std::string("caf\xc3", 4))).is_object());
}

void reads_the_pick()
{
	const ClaudeRoute fast = parse_route_response(200, route_reply("fast"));
	CHECK(fast.ok);
	CHECK(fast.model == kRouteFastModel);
	CHECK(fast.input_tokens == 210);
	CHECK(fast.output_tokens == 12);

	const ClaudeRoute smart = parse_route_response(200, route_reply("smart"));
	CHECK(smart.ok);
	CHECK(smart.model == kRouteSmartModel);
}

void rejects_what_it_cannot_read()
{
	CHECK(!parse_route_response(200, route_reply("medium")).ok);
	CHECK(parse_route_response(200, "not json").error_type == "bad_response");
	CHECK(parse_route_response(200, R"({"content":[{"type":"text","text":"fast"}]})").error_type == "bad_response");
	CHECK(parse_route_response(200, R"({"content":[{"type":"tool_use","name":"route","input":{"model":3}}]})")
		      .error_type == "bad_response");
	CHECK(parse_route_response(500, R"({"error":{"type":7}})").error_type == "bad_response");

	const ClaudeRoute error = parse_route_response(
		529, R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})");
	CHECK(!error.ok);
	CHECK(error.error_type == "overloaded_error");
}

} // namespace

int main()
{
	try {
		request_forces_the_route_tool();
		reads_the_pick();
		rejects_what_it_cannot_read();
	} catch (const std::exception &e) {
		std::printf("threw: %s\n", e.what());
		return 1;
	}
	return check::finish();
}
