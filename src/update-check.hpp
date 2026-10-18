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

#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Asks GitHub once per OBS start whether a newer Hindsight release is out.
// The request runs on its own thread so it never slows OBS down, and any
// failure (offline, private repo, rate limit) just means no notice.

enum class UpdateCheckState {
	not_checked, // turned off in settings, and no one clicked Check for updates
	checking,
	up_to_date,
	newer, // update_check_newer_version() names it
	failed,
};

// Starts a check, unless one is running. Call after curl_global_init, from
// obs_module_load or from the UI thread for the Check for updates button.
// It reads the release channel from Hindsight settings: stable releases
// only, or betas too.
void update_check_start();

UpdateCheckState update_check_state();

// Cancels the check if it is still running and waits for its thread. Call
// from obs_module_unload.
void update_check_stop();

// The newer release's version (such as "0.2.0"), or empty if there is none or
// the check has not finished. Safe to call from any thread.
std::string update_check_newer_version();

// The newer release's Windows installer, as GitHub lists it.
struct UpdateInstaller {
	std::string file_name; // obs-hindsight-<version>-windows-x64-installer.exe
	std::string url;
	std::string sha256; // 64 lowercase hex digits, from GitHub's digest
	uint64_t size = 0;
};

// The newer release's Windows installer, or nothing if there is no newer
// release, it has no installer, or GitHub gave no SHA-256 for it. Safe to call
// from any thread.
std::optional<UpdateInstaller> update_check_installer();

// The page to download the newer release from: a beta's own page on GitHub,
// or for a stable release the link builds set with HINDSIGHT_UPDATE_URL
// (GitHub's latest release without it). Safe to call from any thread.
std::string update_check_release_page();
