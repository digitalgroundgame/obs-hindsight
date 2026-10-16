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
#include "file-hash.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <curl/curl.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace {

// A file the transcriber needs, checked against the hash its publisher lists.
struct ModelFile {
	SpeechModelStatus::Model model;
	const char *file; // under the plugin's config folder
	const char *url;
	const char *sha1;   // either this...
	const char *sha256; // ...or this
	const char *what;   // for the log
};

// Whisper base.en, about 142 MB. The SHA-1 is the one whisper.cpp lists
// (models/README.md at v1.9.5).
constexpr ModelFile kSpeechModel = {SpeechModelStatus::Model::speech,
				    "models/ggml-base.en.bin",
				    "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin",
				    "137c40403d78fd54d454da0f9bd998f78703390c",
				    nullptr,
				    "speech model (about 142 MB)"};

// Silero voice activity detection, about 0.9 MB, so Whisper skips the silence
// in a clip. The model whisper.cpp's README uses at v1.9.5; the SHA-256 is
// the Git LFS object ID Hugging Face lists for the file.
constexpr ModelFile kVadModel = {SpeechModelStatus::Model::voice_detection,
				 "models/ggml-silero-v6.2.0.bin",
				 "https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v6.2.0.bin",
				 nullptr,
				 "2aa269b785eeb53a82983a20501ddf7c1d9c48e33ab63a41391ac6c9f7fb6987",
				 "voice detection model (about 1 MB)"};

std::thread worker;
std::atomic<bool> stopping{false};
std::atomic<bool> running{false};

std::mutex status_lock;
SpeechModelStatus status; // guarded by status_lock

void set_status(SpeechModelStatus::State state, SpeechModelStatus::Model model)
{
	std::lock_guard<std::mutex> guard(status_lock);
	status = SpeechModelStatus{};
	status.state = state;
	status.model = model;
}

std::mutex ready_lock;
std::string ready_speech_path; // guarded by ready_lock
std::string ready_vad_path;    // guarded by ready_lock

std::string config_path(const char *file)
{
	char *path = obs_module_config_path(file);
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

void set_ready(std::string &ready_path, const std::string &path)
{
	std::lock_guard<std::mutex> guard(ready_lock);
	ready_path = path;
}

size_t write_file(char *data, size_t size, size_t count, void *user)
{
	return std::fwrite(data, size, count, static_cast<FILE *>(user)) * size;
}

int on_progress(void *, curl_off_t total, curl_off_t downloaded, curl_off_t, curl_off_t)
{
	{
		std::lock_guard<std::mutex> guard(status_lock);
		status.downloaded = static_cast<long long>(downloaded);
		status.total = static_cast<long long>(total);
	}
	return stopping.load() ? 1 : 0;
}

// Whether the file at path is the model we expect. A captive portal's page,
// a truncated file or a changed upstream file all fail this. Only Windows can
// check the hash for now (see file-hash.hpp); elsewhere the file is trusted.
bool is_expected_model(const ModelFile &model, const std::string &path)
{
	const std::optional<std::string> hash = model.sha1 ? file_sha1(path) : file_sha256(path);
	const char *expected = model.sha1 ? model.sha1 : model.sha256;
#ifdef _WIN32
	return hash && *hash == expected;
#else
	return !hash || *hash == expected;
#endif
}

// Downloads to a .part file and renames it only once complete, so a closed
// OBS or a dropped connection never leaves a half model behind.
bool download(const ModelFile &model, const std::string &path)
{
	const std::string part = path + ".part";
	FILE *file = os_fopen(part.c_str(), "wb");
	if (!file) {
		obs_log(LOG_WARNING, "could not create %s for the %s", part.c_str(), model.what);
		return false;
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		fclose(file);
		os_unlink(part.c_str());
		return false;
	}
	const std::string user_agent = std::string("obs-hindsight/") + PLUGIN_VERSION;
	curl_easy_setopt(curl, CURLOPT_URL, model.url);
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
			obs_log(LOG_WARNING, "could not download the %s: %s", model.what,
				result != CURLE_OK ? curl_easy_strerror(result) : "could not write the file");
		os_unlink(part.c_str());
		return false;
	}
	if (!is_expected_model(model, part)) {
		obs_log(LOG_WARNING, "the downloaded %s is not the expected file; deleted it", model.what);
		os_unlink(part.c_str());
		return false;
	}
	if (os_rename(part.c_str(), path.c_str()) != 0) {
		obs_log(LOG_WARNING, "could not save the %s to %s", model.what, path.c_str());
		os_unlink(part.c_str());
		return false;
	}
	return true;
}

// Makes sure one model is on disk and intact, downloading it if not, then
// publishes its path in ready_path. Returns whether it is ready.
bool fetch(const ModelFile &model, std::string &ready_path)
{
	const std::string path = config_path(model.file);
	if (path.empty())
		return false;
	set_status(SpeechModelStatus::State::checking, model.model);

	// A model from an earlier start is checked again, off the thread that
	// loads OBS, in case it was damaged since.
	if (os_file_exists(path.c_str())) {
		if (is_expected_model(model, path)) {
			set_ready(ready_path, path);
			return true;
		}
		obs_log(LOG_WARNING, "the saved %s is damaged; downloading it again", model.what);
		os_unlink(path.c_str());
	}

	set_status(SpeechModelStatus::State::downloading, model.model);
	obs_log(LOG_INFO, "downloading the %s to %s", model.what, path.c_str());
	const uint64_t start = os_gettime_ns();
	if (!download(model, path))
		return false;
	obs_log(LOG_INFO, "%s downloaded in %.0f s", model.what, static_cast<double>(os_gettime_ns() - start) / 1e9);
	set_ready(ready_path, path);
	return true;
}

void run_downloads()
{
	// The speech model first: transcription works without voice detection,
	// only slower. The status reports the first failure, if any.
	const bool speech_ready = fetch(kSpeechModel, ready_speech_path);
	const bool vad_ready = !stopping.load() && fetch(kVadModel, ready_vad_path);
	if (speech_ready && vad_ready)
		set_status(SpeechModelStatus::State::ready, SpeechModelStatus::Model::speech);
	else
		set_status(SpeechModelStatus::State::failed, speech_ready ? kVadModel.model : kSpeechModel.model);
	running = false;
}

} // namespace

void speech_model_start()
{
	if (running.load())
		return;
	if (worker.joinable())
		worker.join(); // a finished run, before a retry

	char *dir = obs_module_config_path("models");
	if (!dir) {
		set_status(SpeechModelStatus::State::failed, SpeechModelStatus::Model::speech);
		return;
	}
	os_mkdirs(dir);
	bfree(dir);

	stopping = false;
	running = true;
	worker = std::thread(run_downloads);
}

SpeechModelStatus speech_model_status()
{
	std::lock_guard<std::mutex> guard(status_lock);
	return status;
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
	return ready_speech_path;
}

std::string vad_model_path()
{
	std::lock_guard<std::mutex> guard(ready_lock);
	return ready_vad_path;
}
