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

#include "connect-client.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <curl/curl.h>

#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace {

// Digital Ground Game Connect's replies are a few hundred bytes.
constexpr size_t kMaxResponseBytes = 64 * 1024;

// Digital Ground Game Connect gives Discord 12 seconds, so 15 leaves room for
// the trip there and back. No separate connect limit: on Windows one cut off
// requests after the connection was open (see claude-client.cpp).
constexpr long kRequestTimeoutSeconds = 15;

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

// HINDSIGHT_CONNECT_URL, if set, for testing against a local Digital Ground
// Game Connect.
const char *custom_url()
{
	const char *custom = std::getenv("HINDSIGHT_CONNECT_URL");
	return custom && *custom ? custom : nullptr;
}

ConnectReply failed(const char *error, std::string message, bool retry = false)
{
	ConnectReply reply;
	reply.error = error;
	reply.message = std::move(message);
	reply.retry = retry;
	return reply;
}

// One POST to path, with body as JSON (empty for none).
ConnectReply post(const char *path, const std::string &code, const std::string &body, const std::atomic<bool> &cancel)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return failed("network", "curl failed to start");

	curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "content-type: application/json");
	headers = curl_slist_append(headers, ("authorization: Bearer " + code).c_str());
	const char *custom = custom_url();
	const std::string url = std::string(custom ? custom : kConnectUrl) + path;
	const std::string user_agent = std::string("obs-hindsight/") + PLUGIN_VERSION;
	std::string response;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	// Only HTTPS carries the code, unless a local test server is set.
	if (!custom)
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, kRequestTimeoutSeconds);

	const CURLcode result = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (cancel.load())
		return failed("cancelled", {});
	if (result != CURLE_OK)
		return failed("network",
			      std::string("Couldn't reach Digital Ground Game Connect (") + curl_easy_strerror(result) +
				      "). Check your internet connection.",
			      true);
	return parse_connect_response(status, response);
}

// Waits up to seconds, returning early (and false) if cancel is set.
bool wait(int seconds, const std::atomic<bool> &cancel)
{
	for (int tenths = 0; tenths < seconds * 10; tenths++) {
		if (cancel.load())
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	return !cancel.load();
}

std::thread test_worker;
std::atomic<bool> test_stopping{false};
std::mutex test_lock;
ConnectTestStatus test_status; // guarded by test_lock

void run_test(const std::string &code)
{
	ConnectReply reply = post(kConnectTestPath, code, {}, test_stopping);
	if (reply.ok)
		obs_log(LOG_INFO, "Digital Ground Game Connect test: sent a test DM to %s", reply.discord_name.c_str());
	else if (reply.error != "cancelled")
		obs_log(LOG_WARNING, "Digital Ground Game Connect test failed (%s): %s", reply.error.c_str(),
			reply.message.c_str());
	std::lock_guard<std::mutex> guard(test_lock);
	test_status.state = ConnectTestState::done;
	test_status.reply = std::move(reply);
}

} // namespace

ConnectReply connect_send_message(const std::string &code, const std::string &content, const std::atomic<bool> &cancel)
{
	const std::string body = build_connect_message(content, new_connect_message_id());
	ConnectReply reply = post(kConnectMessagesPath, code, body, cancel);
	if (!reply.ok && reply.retry && wait(2, cancel)) {
		obs_log(LOG_WARNING, "sending to Discord failed (%s); trying once more", reply.error.c_str());
		// The same id, so Digital Ground Game Connect sends it once
		// even if the first try got through.
		reply = post(kConnectMessagesPath, code, body, cancel);
	}
	return reply;
}

bool connect_test_start(const std::string &code)
{
	{
		std::lock_guard<std::mutex> guard(test_lock);
		if (test_status.state == ConnectTestState::testing)
			return false;
		test_status = {};
		test_status.state = ConnectTestState::testing;
	}
	if (test_worker.joinable())
		test_worker.join(); // an earlier test, already finished
	test_stopping = false;
	test_worker = std::thread(run_test, code);
	return true;
}

ConnectTestStatus connect_test_status()
{
	std::lock_guard<std::mutex> guard(test_lock);
	return test_status;
}

void connect_test_stop()
{
	test_stopping = true;
	if (test_worker.joinable())
		test_worker.join();
}
