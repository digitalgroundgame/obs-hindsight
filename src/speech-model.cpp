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

#include "speech-model.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <curl/curl.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

namespace {

constexpr const char *kModelFile = "models/ggml-base.en.bin";
constexpr const char *kModelUrl = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin";

std::thread worker;
std::atomic<bool> stopping{false};

std::mutex ready_lock;
std::string ready_path; // guarded by ready_lock

std::string model_path()
{
	char *path = obs_module_config_path(kModelFile);
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

void set_ready(const std::string &path)
{
	std::lock_guard<std::mutex> guard(ready_lock);
	ready_path = path;
}

size_t write_file(char *data, size_t size, size_t count, void *user)
{
	return std::fwrite(data, size, count, static_cast<FILE *>(user)) * size;
}

int on_progress(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	return stopping.load() ? 1 : 0;
}

// Downloads to a .part file and renames it only once complete, so a closed
// OBS or a dropped connection never leaves a half model behind.
bool download(const std::string &path)
{
	const std::string part = path + ".part";
	FILE *file = os_fopen(part.c_str(), "wb");
	if (!file) {
		obs_log(LOG_WARNING, "could not create %s for the speech model", part.c_str());
		return false;
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		fclose(file);
		os_unlink(part.c_str());
		return false;
	}
	const std::string user_agent = std::string("obs-hindsight/") + PLUGIN_VERSION;
	curl_easy_setopt(curl, CURLOPT_URL, kModelUrl);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
	// Hugging Face redirects to its download servers.
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
	// A slow connection is fine; a stalled one is given up on.
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);

	const CURLcode result = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	const bool closed = fclose(file) == 0;

	if (result != CURLE_OK || !closed) {
		if (!stopping.load())
			obs_log(LOG_WARNING, "could not download the speech model: %s",
				result != CURLE_OK ? curl_easy_strerror(result) : "could not write the file");
		os_unlink(part.c_str());
		return false;
	}
	if (os_rename(part.c_str(), path.c_str()) != 0) {
		obs_log(LOG_WARNING, "could not save the speech model to %s", path.c_str());
		os_unlink(part.c_str());
		return false;
	}
	return true;
}

void run_download(const std::string &path)
{
	obs_log(LOG_INFO, "downloading the speech model (about 142 MB) to %s", path.c_str());
	const uint64_t start = os_gettime_ns();
	if (!download(path))
		return;
	obs_log(LOG_INFO, "speech model downloaded in %.0f s", static_cast<double>(os_gettime_ns() - start) / 1e9);
	set_ready(path);
}

} // namespace

void speech_model_start()
{
	if (worker.joinable())
		return;
	const std::string path = model_path();
	if (path.empty())
		return;
	if (os_file_exists(path.c_str())) {
		set_ready(path);
		return;
	}

	char *dir = obs_module_config_path("models");
	if (dir)
		os_mkdirs(dir);
	bfree(dir);

	stopping = false;
	worker = std::thread(run_download, path);
}

void speech_model_stop()
{
	stopping = true;
	if (worker.joinable())
		worker.join();
}

std::string speech_model_path()
{
	std::lock_guard<std::mutex> guard(ready_lock);
	return ready_path;
}

void speech_model_discard()
{
	std::lock_guard<std::mutex> guard(ready_lock);
	if (ready_path.empty())
		return;
	os_unlink(ready_path.c_str());
	obs_log(LOG_WARNING,
		"deleted the speech model because it could not be loaded; restart OBS to download it again");
	ready_path.clear();
}
