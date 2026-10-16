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

#include <obs.h>

// The Update now button: downloads the newer release's Windows installer that
// update-check.cpp found, checks it against the SHA-256 GitHub lists for it,
// and starts it. The installer shows Windows' admin prompt, then offers to
// close OBS, updates the plugin and can start OBS again.

enum class UpdateInstallState {
	idle,        // nothing started yet
	downloading, // fetching and checking the installer
	started,     // the installer was opened (it may since have been cancelled)
	failed,      // the download, the check or starting the installer failed
};

// Whether Update now can work here: Windows, and the update check found an
// installer with a SHA-256.
bool update_install_supported();

// Starts the download on its own thread unless one is running. Clicking
// again after the installer started downloads and starts it again, in case
// the streamer cancelled it. source is the filter whose settings show the
// button; they are refreshed as the state changes.
void update_install_start(obs_source_t *source);

UpdateInstallState update_install_state();

// How much of the installer has downloaded, 0 to 100, or -1 when no
// download is running. Safe to call from any thread.
int update_install_percent();

// Cancels a download and waits for its thread. Call from obs_module_unload,
// before curl is shut down.
void update_install_stop();
