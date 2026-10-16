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

#include "claude-client.hpp"
#include "local-time.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <curl/curl.h>

#include <cstddef>
#include <string>

namespace {

// An answer is a few KB; stop well before anything odd.
constexpr std::size_t kMaxResponseBytes = 256 * 1024;

size_t append_body(char *data, size_t size, size_t count, void *user)
{
	auto *body = static_cast<std::string *>(user);
	const size_t bytes = size * count;
	if (body->size() + bytes > kMaxResponseBytes)
		return 0; // makes curl fail the transfer
	body->append(data, bytes);
	return bytes;
}

// Returning non-zero aborts the transfer.
int on_progress(void *user, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	return static_cast<const std::atomic<bool> *>(user)->load() ? 1 : 0;
}

ClaudeAnswer failed(const char *type, std::string message)
{
	ClaudeAnswer result;
	result.error_type = type;
	result.error_message = std::move(message);
	return result;
}

} // namespace

ClaudeAnswer ask_claude(const std::string &transcript, const std::string &api_key, const std::atomic<bool> &cancel)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return failed("network", "curl failed to start");

	const std::string request = build_claude_request(transcript, local_time_text());
	std::string body;

	curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");
	headers = curl_slist_append(headers, (std::string("anthropic-version: ") + kClaudeApiVersion).c_str());
	headers = curl_slist_append(headers, ("x-api-key: " + api_key).c_str());

	curl_easy_setopt(curl, CURLOPT_URL, kClaudeMessagesUrl);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.size()));
	const std::string user_agent = std::string("obs-hindsight/") + PLUGIN_VERSION;
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

	const CURLcode result = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (cancel.load())
		return failed("cancelled", {});
	if (result != CURLE_OK)
		return failed("network", curl_easy_strerror(result));

	ClaudeAnswer answer = parse_claude_response(status, body);
	if (answer.outcome == ClaudeAnswer::Outcome::failed)
		obs_log(LOG_WARNING, "Claude request failed (HTTP %ld, %s): %s", status, answer.error_type.c_str(),
			answer.error_message.c_str());
	return answer;
}
