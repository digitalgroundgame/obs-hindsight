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

#include "discord-client.hpp"

#include <plugin-support.h>

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace {

// Discord's replies are a few kilobytes.
constexpr size_t kMaxResponseBytes = 64 * 1024;
constexpr long kRequestTimeoutSeconds = 10;
// The longest rate limit Hindsight waits out before giving up on a DM.
constexpr double kMaxRetryAfterSeconds = 5;

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

struct Response {
	bool sent = false; // false for a network failure or cancel
	long status = 0;
	std::string body;
	std::string error; // curl's, when not sent
};

// One POST to path under kDiscordApiUrl with body as JSON.
Response post(const std::string &path, const std::string &bot_token, const std::string &body,
	      const std::atomic<bool> &cancel)
{
	Response response;
	CURL *curl = curl_easy_init();
	if (!curl) {
		response.error = "curl failed to start";
		return response;
	}

	curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");
	headers = curl_slist_append(headers, ("authorization: Bot " + bot_token).c_str());
	const std::string url = std::string(kDiscordApiUrl) + path;
	// Discord asks bots for this form of User-Agent:
	// https://docs.discord.com/developers/reference#user-agent
	const std::string user_agent =
		std::string("DiscordBot (https://github.com/digitalgroundgame/obs-hindsight, ") + PLUGIN_VERSION + ")";

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, kRequestTimeoutSeconds);

	const CURLcode result = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	response.sent = result == CURLE_OK;
	if (!response.sent)
		response.error = curl_easy_strerror(result);
	return response;
}

// Waits up to seconds, returning early (and false) if cancel is set.
bool wait(double seconds, const std::atomic<bool> &cancel)
{
	const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
	while (std::chrono::steady_clock::now() < until) {
		if (cancel.load())
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	return !cancel.load();
}

DiscordReply failed(const char *error, std::string message)
{
	DiscordReply reply;
	reply.error = error;
	reply.message = std::move(message);
	return reply;
}

// Sends one request and reads it with parse, waiting out one short rate
// limit: https://docs.discord.com/developers/topics/rate-limits
DiscordReply call(const std::string &path, const std::string &bot_token, const std::string &body,
		  DiscordReply (*parse)(long, const std::string &), const std::atomic<bool> &cancel)
{
	for (int attempt = 0;; attempt++) {
		const Response response = post(path, bot_token, body, cancel);
		if (cancel.load())
			return failed("cancelled", {});
		if (!response.sent)
			return failed("network", "Couldn't reach Discord (" + response.error +
							 "). Check your internet connection.");
		DiscordReply reply = parse(response.status, response.body);
		if (reply.error != "rate_limited" || attempt > 0 || reply.retry_after > kMaxRetryAfterSeconds)
			return reply;
		// (std::max): curl.h brings in windows.h on Windows, whose max macro
		// would otherwise take the call over.
		if (!wait((std::max)(reply.retry_after, 0.5), cancel))
			return failed("cancelled", {});
	}
}

} // namespace

DiscordReply discord_send_dm(const std::string &bot_token, const std::string &user_id, const std::string &content,
			     const std::atomic<bool> &cancel)
{
	// Discord returns the existing DM channel if there is one, so opening it
	// for each answer is safe; answers are far too rare to hit its limits.
	DiscordReply channel =
		call("/users/@me/channels", bot_token, build_discord_open_dm(user_id), parse_discord_open_dm, cancel);
	if (!channel.ok)
		return channel;
	return call("/channels/" + channel.channel_id + "/messages", bot_token, build_discord_message(content),
		    parse_discord_message, cancel);
}
