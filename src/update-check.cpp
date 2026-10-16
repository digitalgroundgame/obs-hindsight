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
#include "update-asset.hpp"
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
#include <utility>

// Our own site redirects this to the newest release on GitHub, so the link
// can move without a plugin update. Update now still reads GitHub's API
// directly, since that is where the installer's SHA-256 comes from.
const char *const kHindsightReleasesPage = "https://connect.digitalgroundgame.org/hindsight/update";

namespace {

// Only full releases: GitHub leaves drafts and pre-releases out of "latest".
constexpr const char *kLatestReleaseApi =
	"https://api.github.com/repos/digitalgroundgame/obs-hindsight/releases/latest";

// The release JSON is a few KB plus asset details; stop well before anything odd.
constexpr std::size_t kMaxResponseBytes = 1024 * 1024;

std::thread worker;
std::atomic<bool> stopping{false};
std::atomic<UpdateCheckState> state{UpdateCheckState::not_checked};

std::mutex result_lock;
std::string newer_version;                // guarded by result_lock
std::optional<UpdateInstaller> installer; // guarded by result_lock

struct LatestRelease {
	std::string tag;
	std::optional<UpdateInstaller> installer;
};

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

// The release's Windows installer among its files, if GitHub gave a usable
// SHA-256 for it. The Update now button refuses anything else.
std::optional<UpdateInstaller> find_installer(obs_data_t *json, const std::string &tag)
{
	const std::string wanted = windows_installer_name(tag);
	if (wanted.empty())
		return std::nullopt;

	std::optional<UpdateInstaller> found;
	obs_data_array_t *assets = obs_data_get_array(json, "assets");
	const size_t count = assets ? obs_data_array_count(assets) : 0;
	for (size_t i = 0; i < count && !found; i++) {
		obs_data_t *asset = obs_data_array_item(assets, i);
		const std::optional<std::string> sha256 = sha256_from_digest(obs_data_get_string(asset, "digest"));
		const long long size = obs_data_get_int(asset, "size");
		if (wanted == obs_data_get_string(asset, "name") && sha256 && size > 0) {
			found = UpdateInstaller{wanted, obs_data_get_string(asset, "browser_download_url"), *sha256,
						static_cast<uint64_t>(size)};
		}
		obs_data_release(asset);
	}
	obs_data_array_release(assets);
	return found;
}

std::optional<LatestRelease> fetch_latest_release()
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
	// Short limits: OBS waits for this thread when it closes.
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

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
	LatestRelease release;
	release.tag = obs_data_get_string(json, "tag_name");
	release.installer = find_installer(json, release.tag);
	obs_data_release(json);
	return release;
}

void run_check()
{
	std::optional<LatestRelease> release = fetch_latest_release();
	if (!release) {
		state = UpdateCheckState::failed;
		return;
	}

	if (!update_available(release->tag, PLUGIN_VERSION)) {
		obs_log(LOG_INFO, "up to date (latest release is %s)", release->tag.c_str());
		state = UpdateCheckState::up_to_date;
		return;
	}

	obs_log(LOG_INFO, "Hindsight %s is available (running %s): %s", release->tag.c_str(), PLUGIN_VERSION,
		kHindsightReleasesPage);
	std::lock_guard<std::mutex> guard(result_lock);
	newer_version = std::move(release->tag);
	installer = std::move(release->installer);
	state = UpdateCheckState::newer;
}

} // namespace

void update_check_start()
{
	if (state == UpdateCheckState::checking)
		return;
	if (worker.joinable())
		worker.join(); // an earlier check, already finished
	// Without an asynchronous resolver, curl looks up names in one blocking
	// call that ignores our timeouts (CURLOPT_NOSIGNAL turns off the alarm it
	// would use) and never calls on_progress. A slow DNS server could then
	// hold up closing OBS, so skip the check rather than risk that.
	const curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
	if (!(info->features & CURL_VERSION_ASYNCHDNS)) {
		obs_log(LOG_INFO, "update check skipped: this curl build cannot look up names in the background");
		state = UpdateCheckState::failed;
		return;
	}
	stopping = false;
	state = UpdateCheckState::checking;
	worker = std::thread(run_check);
}

void update_check_stop()
{
	stopping = true;
	if (worker.joinable())
		worker.join();
}

UpdateCheckState update_check_state()
{
	return state.load();
}

std::string update_check_newer_version()
{
	std::lock_guard<std::mutex> guard(result_lock);
	return newer_version;
}

std::optional<UpdateInstaller> update_check_installer()
{
	std::lock_guard<std::mutex> guard(result_lock);
	return installer;
}
