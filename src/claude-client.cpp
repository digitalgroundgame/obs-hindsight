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
#include <cstdio>
#include <string>

namespace {

// An answer is a few KB, but the search results Claude read come back with it
// (encrypted), so allow for those; stop well before anything odd.
constexpr std::size_t kMaxResponseBytes = 1024 * 1024;

// How many times to resume a turn the API paused while Claude searched. With
// kClaudeMaxSearches capping the searches, one is plenty; more would only
// make the streamer wait.
constexpr int kMaxResumes = 1;

// The whole request, thinking and searches included, gets a minute. There is
// no shorter limit for opening the connection: in 0.9.0 on Windows a 10 s
// connect limit cut off answers Claude took longer than 10 s to write, though
// the connection was open after 0.1 s. A lookup or connection that fails still
// fails at once, and one that hangs runs into this limit.
constexpr long kRequestTimeoutSeconds = 60;

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

const char *outcome_name(ClaudeAnswer::Outcome outcome)
{
	switch (outcome) {
	case ClaudeAnswer::Outcome::answered:
		return "answered";
	case ClaudeAnswer::Outcome::no_question:
		return "no question";
	case ClaudeAnswer::Outcome::paused:
		return "paused";
	default:
		return "failed";
	}
}

// Seconds from the start of the transfer to a point in it, or 0 if it never
// got there.
double seconds_until(CURL *curl, CURLINFO info)
{
	curl_off_t microseconds = 0;
	if (curl_easy_getinfo(curl, info, &microseconds) != CURLE_OK)
		return 0;
	return static_cast<double>(microseconds) / 1e6;
}

// Where the time in one request went, for the log: looking up the address,
// connecting, the TLS handshake, then Claude's work until the first byte of
// the reply. A step that shows 0 never finished, or was skipped because the
// request went out on a connection that was already open.
std::string timings(CURL *curl)
{
	char line[160];
	std::snprintf(line, sizeof(line), "%.1f s (lookup %.1f, connect %.1f, TLS %.1f, first byte %.1f)",
		      seconds_until(curl, CURLINFO_TOTAL_TIME_T), seconds_until(curl, CURLINFO_NAMELOOKUP_TIME_T),
		      seconds_until(curl, CURLINFO_CONNECT_TIME_T), seconds_until(curl, CURLINFO_APPCONNECT_TIME_T),
		      seconds_until(curl, CURLINFO_STARTTRANSFER_TIME_T));
	return line;
}

// Failures before any of the request went out, so the API never saw it and
// trying again cannot bill it twice. A timeout is not one of them: after a
// minute, the streamer has waited long enough.
bool never_sent(CURLcode result)
{
	return result == CURLE_COULDNT_RESOLVE_HOST || result == CURLE_COULDNT_CONNECT;
}

// Sends one request on curl, which the caller reuses so a resumed turn goes
// out on the connection that is already open. number counts the requests for
// the log.
ClaudeAnswer send(CURL *curl, int number, const std::string &request, const std::string &api_key,
		  const std::atomic<bool> &cancel)
{
	curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");
	headers = curl_slist_append(headers, (std::string("anthropic-version: ") + kClaudeApiVersion).c_str());
	headers = curl_slist_append(headers, ("x-api-key: " + api_key).c_str());
	const std::string user_agent = std::string("obs-hindsight/") + PLUGIN_VERSION;
	std::string body;

	curl_easy_setopt(curl, CURLOPT_URL, kClaudeMessagesUrl);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.size()));
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, kRequestTimeoutSeconds);

	CURLcode result = curl_easy_perform(curl);
	if (result != CURLE_OK && !cancel.load() && never_sent(result)) {
		obs_log(LOG_WARNING, "Claude request %d couldn't connect after %s: %s; trying once more", number,
			timings(curl).c_str(), curl_easy_strerror(result));
		body.clear();
		// curl restarts its timers for each transfer, so the timings logged
		// below cover only this second attempt; the first is in the line above.
		result = curl_easy_perform(curl);
	}
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	const std::string took = timings(curl);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
	curl_slist_free_all(headers);

	if (cancel.load())
		return failed("cancelled", {});
	if (result != CURLE_OK) {
		curl_off_t uploaded = 0;
		curl_easy_getinfo(curl, CURLINFO_SIZE_UPLOAD_T, &uploaded);
		obs_log(LOG_WARNING, "Claude request %d failed after %s, %lld of %zu bytes sent: %s", number,
			took.c_str(), static_cast<long long>(uploaded), request.size(), curl_easy_strerror(result));
		return failed("network", curl_easy_strerror(result));
	}

	ClaudeAnswer answer = parse_claude_response(status, body);
	if (answer.outcome == ClaudeAnswer::Outcome::failed)
		obs_log(LOG_WARNING, "Claude request %d failed (HTTP %ld, %s) after %s: %s", number, status,
			answer.error_type.c_str(), took.c_str(), answer.error_message.c_str());
	else
		obs_log(LOG_INFO, "Claude request %d: HTTP %ld, %s, %lld web searches, in %s", number, status,
			outcome_name(answer.outcome), answer.web_searches, took.c_str());
	return answer;
}

} // namespace

ClaudeAnswer ask_claude(const std::string &transcript, const std::string &api_key, const std::atomic<bool> &cancel)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return failed("network", "curl failed to start");

	const std::string now = local_time_text();
	int requests = 1;
	ClaudeAnswer answer = send(curl, requests, build_claude_request(transcript, now), api_key, cancel);
	long long input_tokens = answer.input_tokens;
	long long output_tokens = answer.output_tokens;
	long long web_searches = answer.web_searches;
	for (int resumes = 0; answer.outcome == ClaudeAnswer::Outcome::paused && resumes < kMaxResumes; resumes++) {
		// max_uses counts per request, so pass on only the searches left.
		const long long searches_left = kClaudeMaxSearches - web_searches;
		if (searches_left <= 0)
			break;
		answer = send(curl, ++requests,
			      build_claude_request(transcript, now, answer.paused_content,
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
	curl_easy_cleanup(curl);
	// Every request was billed, so the cost covers them all.
	answer.input_tokens = input_tokens;
	answer.output_tokens = output_tokens;
	answer.web_searches = web_searches;
	return answer;
}
