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

#include <obs-module.h>
#include <plugin-support.h>

#include <curl/curl.h>

#include <cstddef>
#include <ctime>
#include <string>

namespace {

// An answer is a few KB, but the search results Claude read come back with it
// (encrypted), so allow for those; stop well before anything odd.
constexpr std::size_t kMaxResponseBytes = 1024 * 1024;

// How many times to resume a turn the API paused while Claude searched. With
// kClaudeMaxSearches capping the searches, one is plenty; more would only
// make the streamer wait.
constexpr int kMaxResumes = 1;

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

// The streamer's local date, like "2026-10-16".
std::string today()
{
	const std::time_t now = std::time(nullptr);
	std::tm local{};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	char date[16];
	if (std::strftime(date, sizeof(date), "%Y-%m-%d", &local) == 0)
		return {};
	return date;
}

ClaudeAnswer send(const std::string &request, const std::string &api_key, const std::atomic<bool> &cancel)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return failed("network", "curl failed to start");

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

} // namespace

ClaudeAnswer ask_claude(const std::string &transcript, const std::string &api_key, const std::atomic<bool> &cancel)
{
	const std::string date = today();
	ClaudeAnswer answer = send(build_claude_request(transcript, date), api_key, cancel);
	long long input_tokens = answer.input_tokens;
	long long output_tokens = answer.output_tokens;
	long long web_searches = answer.web_searches;
	for (int resumes = 0; answer.outcome == ClaudeAnswer::Outcome::paused && resumes < kMaxResumes; resumes++) {
		// max_uses counts per request, so pass on only the searches left.
		const long long searches_left = kClaudeMaxSearches - web_searches;
		if (searches_left <= 0)
			break;
		answer = send(build_claude_request(transcript, date, answer.paused_content,
						   static_cast<int>(searches_left)),
			      api_key, cancel);
		input_tokens += answer.input_tokens;
		output_tokens += answer.output_tokens;
		web_searches += answer.web_searches;
	}
	if (answer.outcome == ClaudeAnswer::Outcome::paused) {
		obs_log(LOG_WARNING, "Claude was still searching after the API paused it; giving up");
		answer = failed("bad_response", "stop_reason pause_turn");
	}
	// Every request was billed, so the cost covers them all.
	answer.input_tokens = input_tokens;
	answer.output_tokens = output_tokens;
	answer.web_searches = web_searches;
	return answer;
}
