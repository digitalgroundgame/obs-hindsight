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

#include "update-notice.hpp"
#include "update-check.hpp"
#include "update-install.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <atomic>

namespace {

// Set when Update now was clicked while OBS was live; cleared by the next click.
std::atomic<bool> waits_for_stream_end{false};

std::string with_version(const char *text_key, const std::string &version)
{
	std::string text = obs_module_text(text_key);
	const std::string placeholder = "{version}";
	const size_t at = text.find(placeholder);
	if (at != std::string::npos)
		text.replace(at, placeholder.size(), version);
	return text;
}

} // namespace

UpdateNotice update_notice_current()
{
	UpdateNotice notice;
	const std::string version = update_check_newer_version();
	if (version.empty())
		return notice;
	notice.show = true;

	if (!update_install_supported()) {
		notice.text = with_version("Hindsight.UpdateAvailable", version);
		notice.offer_release_page = true;
		return notice;
	}

	// Once the stream and recording have stopped, drop the "stop first" text.
	if (waits_for_stream_end && !obs_frontend_streaming_active() && !obs_frontend_recording_active())
		waits_for_stream_end = false;

	const UpdateInstallState state = update_install_state();
	const char *text_key = "Hindsight.UpdateReady";
	if (waits_for_stream_end && state == UpdateInstallState::idle) {
		text_key = "Hindsight.UpdateStopStreamFirst";
	} else if (state == UpdateInstallState::downloading) {
		text_key = "Hindsight.UpdateDownloading";
		notice.percent = update_install_percent();
	} else if (state == UpdateInstallState::started) {
		text_key = "Hindsight.UpdateStarted";
	} else if (state == UpdateInstallState::failed) {
		text_key = "Hindsight.UpdateFailed";
		notice.is_error = true;
		notice.offer_release_page = true;
	}
	notice.text = with_version(text_key, version);
	if (notice.percent >= 0) {
		const std::string placeholder = "{percent}";
		const size_t at = notice.text.find(placeholder);
		if (at != std::string::npos)
			notice.text.replace(at, placeholder.size(), std::to_string(notice.percent));
	}
	notice.offer_update_now = true;
	notice.update_now_enabled = state != UpdateInstallState::downloading;
	return notice;
}

void update_notice_update_now(obs_source_t *source)
{
	waits_for_stream_end = obs_frontend_streaming_active() || obs_frontend_recording_active();
	if (!waits_for_stream_end)
		update_install_start(source);
}

std::string update_check_status_text()
{
	switch (update_check_state()) {
	case UpdateCheckState::checking:
		return obs_module_text("Hindsight.UpdateChecking");
	case UpdateCheckState::up_to_date:
		return with_version("Hindsight.UpToDate", PLUGIN_VERSION);
	case UpdateCheckState::failed:
		return obs_module_text("Hindsight.UpdateCheckFailed");
	case UpdateCheckState::not_checked:
	case UpdateCheckState::newer:
		break;
	}
	return {};
}

bool update_check_running()
{
	return update_check_state() == UpdateCheckState::checking;
}
