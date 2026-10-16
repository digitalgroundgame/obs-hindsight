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

#include "update-check.hpp"
#include "version-check.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <curl/curl.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

const char *const kHindsightReleasesPage = "https://github.com/digitalgroundgame/obs-hindsight/releases/latest";

namespace {

// Only full releases: GitHub leaves drafts and pre-releases out of "latest".
constexpr const char *kLatestReleaseApi =
	"https://api.github.com/repos/digitalgroundgame/obs-hindsight/releases/latest";

// The release JSON is a few KB plus asset details; stop well before anything odd.
constexpr std::size_t kMaxResponseBytes = 1024 * 1024;

std::thread worker;
std::atomic<bool> stopping{false};
bool curl_ready = false;

std::mutex result_lock;
std::string newer_version; // guarded by result_lock

size_t append_body(char *data, size_t size, size_t count, void *user)
{
	auto *body = static_cast<std::string *>(user);
	const size_t bytes = size * count;
	if (body->size() + bytes > kMaxResponseBytes)
		return 0; // makes curl fail the transfer
	body->append(data, bytes);
	return bytes;
}

// curl calls this about once a second, and often while data flows. Returning
// non-zero aborts the transfer, which lets update_check_stop() return quickly.
int on_progress(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	return stopping.load() ? 1 : 0;
}

std::optional<std::string> fetch_latest_tag()
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return std::nullopt;

	std::string body;
	curl_slist *headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");

	curl_easy_setopt(curl, CURLOPT_URL, kLatestReleaseApi);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	// GitHub's API rejects requests without a User-Agent.
	const std::string user_agent = std::string("obs-hindsight/") + PLUGIN_VERSION;
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_body);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

	const CURLcode result = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (stopping.load())
		return std::nullopt;
	if (result != CURLE_OK) {
		obs_log(LOG_INFO, "update check skipped: %s", curl_easy_strerror(result));
		return std::nullopt;
	}
	if (status != 200) {
		obs_log(LOG_INFO, "update check skipped: GitHub answered HTTP %ld", status);
		return std::nullopt;
	}

	obs_data_t *json = obs_data_create_from_json(body.c_str());
	if (!json) {
		obs_log(LOG_INFO, "update check skipped: GitHub's answer was not JSON");
		return std::nullopt;
	}
	std::string tag = obs_data_get_string(json, "tag_name");
	obs_data_release(json);
	return tag;
}

void run_check()
{
	const std::optional<std::string> tag = fetch_latest_tag();
	if (!tag)
		return;

	if (!update_available(*tag, PLUGIN_VERSION)) {
		obs_log(LOG_INFO, "up to date (latest release is %s)", tag->c_str());
		return;
	}

	obs_log(LOG_INFO, "Hindsight %s is available (running %s): %s", tag->c_str(), PLUGIN_VERSION,
		kHindsightReleasesPage);
	std::lock_guard<std::mutex> guard(result_lock);
	newer_version = *tag;
}

} // namespace

void update_check_start()
{
	if (worker.joinable())
		return;
	// curl_global_init is not safe to race with other threads, so do it here
	// on the thread that loads the module, before the worker starts.
	curl_ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
	if (!curl_ready) {
		obs_log(LOG_WARNING, "update check skipped: curl failed to start");
		return;
	}
	stopping = false;
	worker = std::thread(run_check);
}

void update_check_stop()
{
	stopping = true;
	if (worker.joinable())
		worker.join();
	if (curl_ready) {
		curl_global_cleanup();
		curl_ready = false;
	}
}

std::string update_check_newer_version()
{
	std::lock_guard<std::mutex> guard(result_lock);
	return newer_version;
}
