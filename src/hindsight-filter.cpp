/*
Hindsight
Copyright (C) 2026 DGG AC Dev Team

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

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kDefaultWindowSeconds = 30;
constexpr int kMinWindowSeconds = 5;
constexpr int kMaxWindowSeconds = 30;

// One filter instance per source it is added to (normally just the mic).
struct HindsightFilter {
	obs_source_t *source = nullptr;
	obs_hotkey_id hotkey = OBS_INVALID_HOTKEY_ID;

	uint32_t sample_rate = 0;
	size_t channels = 0;

	// Mono mix of the source, oldest sample at write_pos once the buffer has
	// wrapped. Guarded by lock: OBS calls filter_audio on its audio thread,
	// while the hotkey and settings run on other threads.
	std::mutex lock;
	std::vector<float> ring;
	size_t write_pos = 0;
	size_t filled = 0;

	// Stored for the next steps; the spike does not use them yet.
	std::string claude_api_key;
	std::string discord_bot_token;
	std::string discord_user_id;
};

// Returns the buffered audio, oldest sample first.
std::vector<float> snapshot(HindsightFilter *filter)
{
	std::lock_guard<std::mutex> guard(filter->lock);
	std::vector<float> out;
	out.reserve(filter->filled);
	const size_t size = filter->ring.size();
	const size_t start = (filter->write_pos + size - filter->filled) % size;
	for (size_t i = 0; i < filter->filled; i++)
		out.push_back(filter->ring[(start + i) % size]);
	return out;
}

void write_le(FILE *file, uint32_t value, int bytes)
{
	for (int i = 0; i < bytes; i++)
		fputc((value >> (8 * i)) & 0xff, file);
}

// Writes 16-bit mono PCM, which any audio player opens.
bool write_wav(const std::string &path, const std::vector<float> &samples, uint32_t sample_rate)
{
	FILE *file = os_fopen(path.c_str(), "wb");
	if (!file)
		return false;

	const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 2);
	fwrite("RIFF", 1, 4, file);
	write_le(file, 36 + data_bytes, 4);
	fwrite("WAVEfmt ", 1, 8, file);
	write_le(file, 16, 4);              // fmt chunk size
	write_le(file, 1, 2);               // PCM
	write_le(file, 1, 2);               // mono
	write_le(file, sample_rate, 4);     // sample rate
	write_le(file, sample_rate * 2, 4); // byte rate
	write_le(file, 2, 2);               // block align
	write_le(file, 16, 2);              // bits per sample
	fwrite("data", 1, 4, file);
	write_le(file, data_bytes, 4);

	for (float sample : samples) {
		const float clamped = std::clamp(sample, -1.0f, 1.0f);
		write_le(file, static_cast<uint16_t>(static_cast<int16_t>(clamped * 32767.0f)), 2);
	}

	return fclose(file) == 0;
}

std::string clip_path()
{
	char name[64];
	const time_t now = time(nullptr);
	strftime(name, sizeof(name), "clips/hindsight-%Y%m%d-%H%M%S.wav", localtime(&now));

	char *path = obs_module_config_path(name);
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

// What the hotkey will eventually do end to end. For the spike it saves the
// buffered audio so we can listen to exactly what would be transcribed.
void capture(HindsightFilter *filter)
{
	std::vector<float> samples = snapshot(filter);
	if (samples.empty()) {
		obs_log(LOG_WARNING, "no audio buffered yet; is the filter on an active mic?");
		return;
	}

	const uint32_t sample_rate = filter->sample_rate;
	const std::string path = clip_path();

	// Writing a few megabytes to disk should not hold up the hotkey thread.
	std::thread([samples = std::move(samples), sample_rate, path]() {
		char *dir = obs_module_config_path("clips");
		os_mkdirs(dir);
		bfree(dir);

		const double seconds = static_cast<double>(samples.size()) / sample_rate;
		if (write_wav(path, samples, sample_rate))
			obs_log(LOG_INFO, "saved %.1f s of audio to %s", seconds, path.c_str());
		else
			obs_log(LOG_ERROR, "could not write %s", path.c_str());
	}).detach();
}

void on_hotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		capture(static_cast<HindsightFilter *>(data));
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

	std::lock_guard<std::mutex> guard(filter->lock);
	filter->claude_api_key = obs_data_get_string(settings, "claude_api_key");
	filter->discord_bot_token = obs_data_get_string(settings, "discord_bot_token");
	filter->discord_user_id = obs_data_get_string(settings, "discord_user_id");

	if (filter->ring.size() != size) {
		filter->ring.assign(size, 0.0f);
		filter->write_pos = 0;
		filter->filled = 0;
	}
}

void *create(obs_data_t *settings, obs_source_t *source)
{
	auto *filter = new HindsightFilter();
	filter->source = source;

	audio_t *audio = obs_get_audio();
	filter->sample_rate = audio_output_get_sample_rate(audio);
	filter->channels = audio_output_get_channels(audio);

	update(filter, settings);

	filter->hotkey = obs_hotkey_register_source(source, "hindsight.ask", obs_module_text("Hindsight.AskHotkey"),
						    on_hotkey, filter);
	return filter;
}

void destroy(void *data)
{
	auto *filter = static_cast<HindsightFilter *>(data);
	if (filter->hotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(filter->hotkey);
	delete filter;
}

obs_audio_data *filter_audio(void *data, obs_audio_data *audio)
{
	auto *filter = static_cast<HindsightFilter *>(data);
	float **planes = reinterpret_cast<float **>(audio->data);

	std::lock_guard<std::mutex> guard(filter->lock);
	const size_t size = filter->ring.size();
	if (size == 0 || filter->channels == 0)
		return audio;

	for (uint32_t frame = 0; frame < audio->frames; frame++) {
		float sum = 0.0f;
		for (size_t ch = 0; ch < filter->channels; ch++)
			sum += planes[ch] ? planes[ch][frame] : 0.0f;

		filter->ring[filter->write_pos] = sum / static_cast<float>(filter->channels);
		filter->write_pos = (filter->write_pos + 1) % size;
		filter->filled = std::min(filter->filled + 1, size);
	}

	// The audio passes through untouched; Hindsight only listens.
	return audio;
}

void get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "window_seconds", kDefaultWindowSeconds);
}

obs_properties_t *get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *note =
		obs_properties_add_text(props, "spike_note", obs_module_text("Hindsight.SpikeNote"), OBS_TEXT_INFO);
	obs_property_text_set_info_type(note, OBS_TEXT_INFO_NORMAL);

	obs_properties_add_text(props, "claude_api_key", obs_module_text("Hindsight.ClaudeApiKey"), OBS_TEXT_PASSWORD);
	obs_properties_add_text(props, "discord_bot_token", obs_module_text("Hindsight.DiscordBotToken"),
				OBS_TEXT_PASSWORD);
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

	obs_register_source(&info);
}
