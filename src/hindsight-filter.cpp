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

#include "hindsight-filter.hpp"
#include "audio-freshness.hpp"
#include "audio-ring.hpp"
#include "clip-file.hpp"
#include "secret-store.hpp"
#include "update-check.hpp"
#include "update-install.hpp"
#include "wav.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>
#include <util/task.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kDefaultWindowSeconds = 30;
constexpr int kMinWindowSeconds = 5;
constexpr int kMaxWindowSeconds = 30;

// Clips waiting to be written.
constexpr size_t kMaxPendingClips = 4;

// One filter instance per source it is added to (normally just the mic).
struct HindsightFilter {
	obs_source_t *source = nullptr;
	obs_hotkey_id hotkey = OBS_INVALID_HOTKEY_ID;

	uint32_t sample_rate = 0;
	size_t channels = 0;

	// Mono mix of the source. Guarded by lock: OBS calls filter_audio on its
	// audio thread, while the hotkey and settings run on other threads.
	std::mutex lock;
	AudioRing ring;
	uint64_t last_push_ns = 0; // when the newest buffered audio arrived

	// The window setting, for log messages.
	std::atomic<int> window_seconds{kDefaultWindowSeconds};

	// The API keys are not here: every filter shares the ones in
	// secret-store.cpp.
	std::string discord_user_id;

	// Writes clips one at a time, off the hotkey thread. destroy() finishes
	// the queued clips before the filter goes away.
	os_task_queue_t *clip_writer = nullptr;
	std::atomic<size_t> pending_clips{0};
};

// One clip waiting for the clip writer.
struct ClipJob {
	HindsightFilter *filter;
	std::vector<float> samples;
	uint32_t sample_rate;
	std::string name; // without the extension
};

// Returns the audio heard within the window before now, oldest sample first.
// filter_audio needs the same lock on OBS's audio thread, so the few
// megabytes are allocated before taking it and the lock is held only for the
// copy.
std::vector<float> snapshot(HindsightFilter *filter)
{
	std::vector<float> out;
	const uint64_t now = os_gettime_ns();
	for (;;) {
		size_t capacity = 0;
		{
			std::lock_guard<std::mutex> guard(filter->lock);
			// Retry if the ring grew since we reserved (the slider moved
			// between the two locks), so the copy never allocates here.
			if (out.capacity() >= filter->ring.capacity()) {
				// If the mic has been quiet in OBS (its scene not
				// showing), copy only what is still inside the window.
				const size_t fresh = fresh_sample_count(filter->ring.size(), filter->ring.capacity(),
									filter->sample_rate, filter->last_push_ns, now);
				filter->ring.copy_to(out, fresh);
				break;
			}
			capacity = filter->ring.capacity();
		}
		out.reserve(capacity);
	}
	return out;
}

// Writes an encoded WAV file to file, and closes it.
bool write_wav(FILE *file, const std::vector<uint8_t> &bytes)
{
	const bool written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
	return fclose(file) == 0 && written;
}

// Returns the clips folder, or "" if OBS cannot give the plugin a config
// folder.
std::string clips_folder()
{
	char *dir = obs_module_config_path("clips");
	if (!dir)
		return "";
	os_mkdirs(dir);
	const std::string folder = dir;
	bfree(dir);
	return folder;
}

// Runs on the clip writer's thread. Each filter has its own clip writer, so
// two filters can save a clip of the same name at once; create_new_file
// gives each its own file.
void write_clip(void *param)
{
	auto *job = static_cast<ClipJob *>(param);
	const std::string folder = clips_folder();
	const double seconds = static_cast<double>(job->samples.size()) / job->sample_rate;

	// Encoded before a name is taken, so a clip too long for WAV (encoded as
	// nothing) leaves no empty file behind.
	const std::vector<uint8_t> bytes = encode_wav(job->samples, job->sample_rate);

	if (folder.empty()) {
		obs_log(LOG_ERROR, "could not find the plugin's config folder to save the clip");
	} else if (bytes.empty()) {
		obs_log(LOG_ERROR, "%.1f s of audio is too long to save as a WAV file", seconds);
	} else {
		NewFile clip = create_new_file(folder, job->name, ".wav");
		if (!clip.file)
			obs_log(LOG_ERROR, "could not create a file for %s.wav in %s", job->name.c_str(),
				folder.c_str());
		else if (write_wav(clip.file, bytes))
			obs_log(LOG_INFO, "saved %.1f s of audio to %s", seconds, clip.path.c_str());
		else
			obs_log(LOG_ERROR, "could not write %s", clip.path.c_str());
	}

	job->filter->pending_clips--;
	delete job;
}

// What the hotkey will eventually do end to end. For the spike it saves the
// buffered audio so we can listen to exactly what would be transcribed.
void capture(HindsightFilter *filter)
{
	// Each clip holds up to 30 s of audio (about 6 MB), so don't let them
	// pile up if the disk is slow.
	if (++filter->pending_clips > kMaxPendingClips) {
		filter->pending_clips--;
		obs_log(LOG_WARNING, "still saving earlier clips; skipped this one");
		return;
	}

	std::vector<float> samples = snapshot(filter);
	if (samples.empty()) {
		filter->pending_clips--;
		obs_log(LOG_WARNING, "no audio from the last %d s; is this filter on an active mic?",
			filter->window_seconds.load());
		return;
	}

	// Named for when the hotkey was pressed, the way OBS names recordings.
	char *name = os_generate_formatted_filename(nullptr, false, "hindsight-%CCYY%MM%DD-%hh%mm%ss");
	auto *job = new ClipJob{filter, std::move(samples), filter->sample_rate, name ? name : "hindsight"};
	bfree(name);

	// Writing a few megabytes to disk should not hold up the hotkey thread.
	if (!os_task_queue_queue_task(filter->clip_writer, write_clip, job)) {
		filter->pending_clips--;
		delete job;
		obs_log(LOG_ERROR, "could not queue the clip to be saved");
	}
}

void on_hotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		capture(static_cast<HindsightFilter *>(data));
}

// Keys are entered under Tools > Hindsight settings. Version 0.1.0 kept them in
// the filter settings instead; move any found there into the shared secret
// store (unless a key is already saved there), then remove them from the
// settings and so from the scene collection.
void take_keys_from_settings(obs_data_t *settings)
{
	Secrets old;
	old.claude_api_key = obs_data_get_string(settings, kClaudeApiKeyName);
	old.discord_bot_token = obs_data_get_string(settings, kDiscordBotTokenName);
	if (old.claude_api_key.empty() && old.discord_bot_token.empty())
		return;

	// Leave the keys in the settings rather than lose them.
	if (!secret_store_take_old_keys(old))
		return;
	obs_data_erase(settings, kClaudeApiKeyName);
	obs_data_erase(settings, kDiscordBotTokenName);
}

bool on_save_clip_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	capture(static_cast<HindsightFilter *>(data));
	return false;
}

const char *get_name(void *)
{
	return obs_module_text("Hindsight");
}

void update(void *data, obs_data_t *settings)
{
	auto *filter = static_cast<HindsightFilter *>(data);

	const int seconds = std::clamp(static_cast<int>(obs_data_get_int(settings, "window_seconds")),
				       kMinWindowSeconds, kMaxWindowSeconds);
	const size_t size = static_cast<size_t>(seconds) * filter->sample_rate;
	filter->window_seconds = seconds;

	take_keys_from_settings(settings);

	// A resized ring is a few megabytes. Build it before taking the lock the
	// audio thread waits on, then copy the newest audio into it under the
	// lock so changing the window keeps what was heard. The old one is freed
	// with `resized` after the lock is released.
	bool resize = false;
	{
		std::lock_guard<std::mutex> guard(filter->lock);
		resize = filter->ring.capacity() != size;
	}
	AudioRing resized;
	if (resize)
		resized.reset(size);

	std::string discord_user_id = obs_data_get_string(settings, "discord_user_id");

	std::lock_guard<std::mutex> guard(filter->lock);
	filter->discord_user_id = std::move(discord_user_id);

	if (resize && filter->ring.capacity() != size) {
		resized.take_newest_from(filter->ring);
		std::swap(filter->ring, resized);
	}
}

void *create(obs_data_t *settings, obs_source_t *source)
{
	auto *filter = new HindsightFilter();
	filter->source = source;
	filter->clip_writer = os_task_queue_create();

	audio_t *audio = obs_get_audio();
	filter->sample_rate = audio_output_get_sample_rate(audio);
	filter->channels = audio_output_get_channels(audio);

	update(filter, settings);
	return filter;
}

void unregister_hotkey(HindsightFilter *filter)
{
	if (filter->hotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(filter->hotkey);
	filter->hotkey = OBS_INVALID_HOTKEY_ID;
}

// The hotkey goes on the source the filter is added to, not on the filter.
// OBS loads saved filters as private sources, which cannot have hotkeys, so a
// hotkey on the filter vanished after a restart. On the mic it shows under the
// mic's name in Settings > Hotkeys and is saved with the mic.
void filter_add(void *data, obs_source_t *parent)
{
	auto *filter = static_cast<HindsightFilter *>(data);
	unregister_hotkey(filter);
	filter->hotkey = obs_hotkey_register_source(parent, "hindsight.ask", obs_module_text("Hindsight.AskHotkey"),
						    on_hotkey, filter);
	if (filter->hotkey == OBS_INVALID_HOTKEY_ID)
		obs_log(LOG_WARNING, "could not add the hotkey to '%s'", obs_source_get_name(parent));
}

void filter_remove(void *data, obs_source_t *)
{
	unregister_hotkey(static_cast<HindsightFilter *>(data));
}

void destroy(void *data)
{
	auto *filter = static_cast<HindsightFilter *>(data);
	unregister_hotkey(filter);
	// Writes the clips still queued, then stops the thread.
	os_task_queue_destroy(filter->clip_writer);
	delete filter;
}

obs_audio_data *filter_audio(void *data, obs_audio_data *audio)
{
	auto *filter = static_cast<HindsightFilter *>(data);
	float **planes = reinterpret_cast<float **>(audio->data);

	const uint64_t now = os_gettime_ns();

	std::lock_guard<std::mutex> guard(filter->lock);
	// After a pause (the source was inactive), start over rather than join
	// old audio to new with no gap between them.
	if (audio_resumed_after_gap(filter->last_push_ns, now))
		filter->ring.clear();
	filter->ring.push_planar(planes, filter->channels, audio->frames);
	filter->last_push_ns = now;

	// The audio passes through untouched; Hindsight only listens.
	return audio;
}

void get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "window_seconds", kDefaultWindowSeconds);
}

bool on_download_update_clicked(obs_properties_t *, obs_property_t *, void *)
{
	return false; // OBS opens the button's URL itself
}

// The installer closes OBS, so never start it while OBS is live.
std::atomic<bool> update_waits_for_stream_end{false};

bool on_update_now_clicked(obs_properties_t *, obs_property_t *, void *data)
{
	auto *filter = static_cast<HindsightFilter *>(data);
	update_waits_for_stream_end = obs_frontend_streaming_active() || obs_frontend_recording_active();
	if (!update_waits_for_stream_end)
		update_install_start(filter->source);
	return true; // redraw the settings to show the new state
}

std::string with_version(const char *text_key, const std::string &version)
{
	std::string text = obs_module_text(text_key);
	const std::string placeholder = "{version}";
	const size_t at = text.find(placeholder);
	if (at != std::string::npos)
		text.replace(at, placeholder.size(), version);
	return text;
}

// The notice and Update now button for the current UpdateInstallState.
// Returns whether to also offer the release page in the browser.
bool add_update_now(obs_properties_t *props, HindsightFilter *filter, const std::string &version)
{
	const UpdateInstallState state = update_install_state();
	const char *text_key = "Hindsight.UpdateReady";
	bool offer_page = false;
	if (update_waits_for_stream_end && state == UpdateInstallState::idle) {
		text_key = "Hindsight.UpdateStopStreamFirst";
	} else if (state == UpdateInstallState::downloading) {
		text_key = "Hindsight.UpdateDownloading";
	} else if (state == UpdateInstallState::started) {
		text_key = "Hindsight.UpdateStarted";
	} else if (state == UpdateInstallState::failed) {
		text_key = "Hindsight.UpdateFailed";
		offer_page = true;
	}

	obs_property_t *notice =
		obs_properties_add_text(props, "update_notice", with_version(text_key, version).c_str(), OBS_TEXT_INFO);
	obs_property_text_set_info_type(notice, state == UpdateInstallState::failed ? OBS_TEXT_INFO_ERROR
										    : OBS_TEXT_INFO_WARNING);

	obs_property_t *button = obs_properties_add_button2(props, "update_now", obs_module_text("Hindsight.UpdateNow"),
							    on_update_now_clicked, filter);
	obs_property_set_enabled(button,
				 state != UpdateInstallState::downloading && state != UpdateInstallState::started);
	return offer_page;
}

// Shown at the top of the filter's settings when update-check.cpp found a
// newer release. On Windows, Update now downloads and starts the installer
// (update-install.cpp); elsewhere, or if that fails, a button opens the
// release page. OBS_BUTTON_URL buttons open their URL in the browser:
// https://github.com/obsproject/obs-studio/blob/31.1.1/libobs/obs-properties.h
void add_update_notice(obs_properties_t *props, HindsightFilter *filter)
{
	const std::string version = update_check_newer_version();
	if (version.empty())
		return;

	bool offer_page = true;
	if (update_install_supported() && filter) {
		offer_page = add_update_now(props, filter, version);
	} else {
		obs_property_t *notice = obs_properties_add_text(
			props, "update_notice", with_version("Hindsight.UpdateAvailable", version).c_str(),
			OBS_TEXT_INFO);
		obs_property_text_set_info_type(notice, OBS_TEXT_INFO_WARNING);
	}
	if (!offer_page)
		return;

	obs_property_t *button = obs_properties_add_button2(props, "download_update",
							    obs_module_text("Hindsight.DownloadUpdate"),
							    on_download_update_clicked, nullptr);
	obs_property_button_set_type(button, OBS_BUTTON_URL);
	std::string url = kHindsightReleasesPage; // the setter wants a non-const char * and copies it
	obs_property_button_set_url(button, url.data());
}

obs_properties_t *get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	add_update_notice(props, static_cast<HindsightFilter *>(data));

	obs_property_t *note =
		obs_properties_add_text(props, "spike_note", obs_module_text("Hindsight.SpikeNote"), OBS_TEXT_INFO);
	obs_property_text_set_info_type(note, OBS_TEXT_INFO_NORMAL);

	obs_property_t *keys_note =
		obs_properties_add_text(props, "keys_note", obs_module_text("Hindsight.KeysNote"), OBS_TEXT_INFO);
	obs_property_text_set_info_type(keys_note, OBS_TEXT_INFO_NORMAL);

	obs_properties_add_text(props, "discord_user_id", obs_module_text("Hindsight.DiscordUserId"), OBS_TEXT_DEFAULT);
	obs_properties_add_int_slider(props, "window_seconds", obs_module_text("Hindsight.WindowSeconds"),
				      kMinWindowSeconds, kMaxWindowSeconds, 1);
	obs_properties_add_button2(props, "save_clip", obs_module_text("Hindsight.SaveClip"), on_save_clip_clicked,
				   data);

	return props;
}

} // namespace

void register_hindsight_filter()
{
	obs_source_info info = {};
	info.id = "hindsight_filter";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_AUDIO;
	info.get_name = get_name;
	info.create = create;
	info.destroy = destroy;
	info.update = update;
	info.get_defaults = get_defaults;
	info.get_properties = get_properties;
	info.filter_audio = filter_audio;
	info.filter_add = filter_add;
	info.filter_remove = filter_remove;

	obs_register_source(&info);
}
