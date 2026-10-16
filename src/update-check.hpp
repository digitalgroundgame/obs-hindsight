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

#include <string>

// Asks GitHub once per OBS start whether a newer Hindsight release is out.
// The request runs on its own thread so it never slows OBS down, and any
// failure (offline, private repo, rate limit) just means no notice.

// Starts the check. Call from obs_module_load, after curl_global_init.
void update_check_start();

// Cancels the check if it is still running and waits for its thread. Call
// from obs_module_unload.
void update_check_stop();

// The newer release's version (such as "0.2.0"), or empty if there is none or
// the check has not finished. Safe to call from any thread.
std::string update_check_newer_version();

// The page to download the newest release from.
extern const char *const kHindsightReleasesPage;
