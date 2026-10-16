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

#include <string>

// What to show about a newer Hindsight release, wherever it is shown: the
// filter's settings and the Tools > Hindsight settings window both use this,
// so they always agree.
struct UpdateNotice {
	bool show = false;               // a newer release is out
	std::string text;                // the notice, with the version filled in
	bool is_error = false;           // the last Update now failed
	bool offer_update_now = false;   // Windows, with an installer to download
	bool update_now_enabled = false; // false while downloading
	bool offer_release_page = false; // a link to the release page instead
	int percent = -1;                // 0 to 100 while downloading, else -1
};

UpdateNotice update_notice_current();

// When there is no notice, the settings window offers Check for updates and
// shows how the last check went (empty before any check).
std::string update_check_status_text();
bool update_check_running();

// The Update now button. Starts update-install.cpp's download, unless OBS is
// streaming or recording: the installer closes OBS, so the notice asks the
// streamer to stop first. source is the filter whose settings to refresh as
// the download goes, or null.
void update_notice_update_now(obs_source_t *source);
