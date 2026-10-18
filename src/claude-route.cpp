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

#include "claude-route.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {

constexpr const char *kRouteTool = "route";

const char *const kRoutePrompt =
	"You get an automatic transcript of the last few seconds of a live streamer's microphone. Another model "
	"will answer the most recent question in it. Pick which one by calling the route tool:\n"
	"- fast: quick facts, definitions, dates, current events, casual questions, or no clear question.\n"
	"- smart: questions that need several steps of reasoning, math, code, careful comparison or nuanced "
	"explanation.\n"
	"When unsure, pick fast.";

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

} // namespace

std::string build_route_request(const std::string &transcript)
{
	const json input_schema = {
		{"type", "object"},
		{"properties", {{"model", {{"type", "string"}, {"enum", {"fast", "smart"}}}}}},
		{"required", {"model"}},
		{"additionalProperties", false},
	};
	const json request = {
		{"model", kRouteModel},
		{"max_tokens", 100},
		{"output_config", {{"effort", "low"}}},
		{"system", kRoutePrompt},
		{"tools", json::array({{
				  {"name", kRouteTool},
				  {"description", "Choose the model that answers the question."},
				  {"input_schema", input_schema},
				  {"strict", true},
			  }})},
		{"tool_choice", {{"type", "tool"}, {"name", kRouteTool}}},
		{"messages", json::array({{
				     {"role", "user"},
				     {"content", "<transcript>\n" + transcript + "\n</transcript>"},
			     }})},
	};
	// Replace bad UTF-8 from the transcript rather than throw.
	return request.dump(-1, ' ', false, json::error_handler_t::replace);
}

ClaudeRoute parse_route_response(long status, const std::string &body)
{
	ClaudeRoute route;
	const json reply = json::parse(body, nullptr, false);
	if (!reply.is_object()) {
		route.error_type = "bad_response";
		return route;
	}
	const auto usage = reply.find("usage");
	if (usage != reply.end() && usage->is_object()) {
		route.input_tokens = count_at(*usage, "input_tokens");
		route.output_tokens = count_at(*usage, "output_tokens");
	}
	if (status != 200) {
		const auto error = reply.find("error");
		route.error_type = "bad_response";
		if (error != reply.end() && error->is_object()) {
			const auto type = error->find("type");
			if (type != error->end() && type->is_string())
				route.error_type = type->get<std::string>();
		}
		return route;
	}

	const auto content = reply.find("content");
	if (content != reply.end() && content->is_array()) {
		for (const json &block : *content) {
			if (!block.is_object() || string_at(block, "type") != "tool_use" ||
			    string_at(block, "name") != kRouteTool)
				continue;
			const auto input = block.find("input");
			if (input == block.end() || !input->is_object())
				break;
			const std::string pick = string_at(*input, "model");
			if (pick == "fast" || pick == "smart") {
				route.ok = true;
				route.model = pick == "smart" ? kRouteSmartModel : kRouteFastModel;
				return route;
			}
			break;
		}
	}
	route.error_type = "bad_response";
	return route;
}
