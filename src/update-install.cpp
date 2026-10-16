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

#include "update-install.hpp"
#include "update-check.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#ifdef _WIN32
#include "sha256.hpp"

#include <curl/curl.h>

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdint>
#include <cstdio>
#include <string>
#endif

namespace {

std::thread worker;
std::atomic<bool> stopping{false};
std::atomic<UpdateInstallState> state{UpdateInstallState::idle};

std::mutex source_lock;
obs_weak_source_t *source_to_refresh = nullptr; // guarded by source_lock

#ifdef _WIN32

// Redraws the filter's settings, if they are open, to show the new state.
// OBS hands the redraw to its UI thread, so this is safe from the worker.
void set_state(UpdateInstallState next)
{
	state = next;
	std::lock_guard<std::mutex> guard(source_lock);
	obs_source_t *source = obs_weak_source_get_source(source_to_refresh);
	if (source) {
		obs_source_update_properties(source);
		obs_source_release(source);
	}
}

struct Download {
	std::FILE *file = nullptr;
	Sha256 hash;
	uint64_t received = 0;
	uint64_t expected = 0;
};

size_t write_chunk(char *data, size_t size, size_t count, void *user)
{
	auto *download = static_cast<Download *>(user);
	const size_t bytes = size * count;
	// More than GitHub said the file holds: stop rather than keep writing.
	if (download->received + bytes > download->expected)
		return 0;
	if (std::fwrite(data, 1, bytes, download->file) != bytes)
		return 0;
	download->hash.add(data, bytes);
	download->received += bytes;
	return bytes;
}

int on_progress(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	return stopping.load() ? 1 : 0;
}

// %TEMP%\obs-hindsight-update\<installer file name>. The name comes from
// windows_installer_name, so it is plain ASCII with no path separators.
std::wstring installer_path(const std::string &file_name)
{
	wchar_t temp[MAX_PATH + 1];
	const DWORD length = GetTempPathW(MAX_PATH + 1, temp);
	if (length == 0 || length > MAX_PATH)
		return {};
	std::wstring folder(temp, length);
	folder += L"obs-hindsight-update";
	if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
		return {};
	return folder + L"\\" + std::wstring(file_name.begin(), file_name.end());
}

// Downloads the installer to path, hashing it on the way. True only when the
// whole file arrived and its SHA-256 matches the one GitHub lists.
bool download_installer(const UpdateInstaller &installer, const std::wstring &path)
{
	Download download;
	download.expected = installer.size;
	if (_wfopen_s(&download.file, path.c_str(), L"wb") != 0 || !download.file) {
		obs_log(LOG_WARNING, "update: could not create the installer file");
		return false;
	}

	CURL *curl = curl_easy_init();
	CURLcode result = CURLE_FAILED_INIT;
	long status = 0;
	if (curl) {
		const std::string user_agent = std::string("obs-hindsight/") + PLUGIN_VERSION;
		curl_easy_setopt(curl, CURLOPT_URL, installer.url.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
		// GitHub answers release downloads with a redirect to its file host.
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
		curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, on_progress);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
		// Give up on a stalled download (under 1 KB/s for 30 seconds)
		// rather than on a fixed time, so slow connections still finish.
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
		curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
		result = curl_easy_perform(curl);
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
		curl_easy_cleanup(curl);
	}
	const bool closed = std::fclose(download.file) == 0;

	if (stopping.load())
		return false;
	if (result != CURLE_OK) {
		obs_log(LOG_WARNING, "update: download failed: %s", curl_easy_strerror(result));
		return false;
	}
	if (status != 200 || !closed || download.received != installer.size) {
		obs_log(LOG_WARNING, "update: download failed (HTTP %ld, %llu of %llu bytes)", status,
			static_cast<unsigned long long>(download.received),
			static_cast<unsigned long long>(installer.size));
		return false;
	}
	const std::string sha256 = download.hash.finish();
	if (sha256 != installer.sha256) {
		obs_log(LOG_WARNING, "update: the installer's SHA-256 (%s) is not the one GitHub lists (%s)",
			sha256.c_str(), installer.sha256.c_str());
		return false;
	}
	return true;
}

// Starts the installer. It asks for admin rights itself. /OBS= tells it which
// OBS to offer to start again when it finishes (see installer-Windows.iss).
bool launch_installer(const std::wstring &path)
{
	wchar_t obs[32768];
	const DWORD length = GetModuleFileNameW(nullptr, obs, 32768);
	std::wstring parameters;
	if (length > 0 && length < 32768)
		parameters = L"/OBS=\"" + std::wstring(obs, length) + L"\"";

	// ShellExecuteEx may use COM, so the calling thread needs it set up:
	// https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw
	const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	SHELLEXECUTEINFOW info = {};
	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOASYNC;
	info.lpVerb = L"open";
	info.lpFile = path.c_str();
	info.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
	info.nShow = SW_SHOWNORMAL;
	const bool started = ShellExecuteExW(&info) != FALSE;
	const DWORD error = started ? 0 : GetLastError();

	if (SUCCEEDED(com))
		CoUninitialize();
	if (!started)
		obs_log(LOG_WARNING, "update: could not start the installer (error %lu)", error);
	return started;
}

void run_update(UpdateInstaller installer)
{
	const std::wstring path = installer_path(installer.file_name);
	if (path.empty()) {
		obs_log(LOG_WARNING, "update: could not find the temp folder");
		set_state(UpdateInstallState::failed);
		return;
	}

	obs_log(LOG_INFO, "update: downloading %s", installer.url.c_str());
	if (!download_installer(installer, path)) {
		DeleteFileW(path.c_str());
		set_state(stopping.load() ? UpdateInstallState::idle : UpdateInstallState::failed);
		return;
	}

	obs_log(LOG_INFO, "update: SHA-256 matches; starting the installer");
	set_state(launch_installer(path) ? UpdateInstallState::started : UpdateInstallState::failed);
}

#endif

} // namespace

bool update_install_supported()
{
#ifdef _WIN32
	return update_check_installer().has_value();
#else
	return false;
#endif
}

void update_install_start(obs_source_t *source)
{
#ifdef _WIN32
	const UpdateInstallState current = state.load();
	if (current == UpdateInstallState::downloading || current == UpdateInstallState::started)
		return;
	std::optional<UpdateInstaller> installer = update_check_installer();
	if (!installer)
		return;
	if (worker.joinable())
		worker.join(); // a finished attempt that failed

	{
		std::lock_guard<std::mutex> guard(source_lock);
		obs_weak_source_release(source_to_refresh);
		source_to_refresh = obs_source_get_weak_source(source);
	}
	stopping = false;
	state = UpdateInstallState::downloading;
	worker = std::thread(run_update, std::move(*installer));
#else
	(void)source;
#endif
}

UpdateInstallState update_install_state()
{
	return state.load();
}

void update_install_stop()
{
	stopping = true;
	if (worker.joinable())
		worker.join();
	std::lock_guard<std::mutex> guard(source_lock);
	obs_weak_source_release(source_to_refresh);
	source_to_refresh = nullptr;
}
