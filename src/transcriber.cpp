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

#include "transcriber.hpp"
#include "speech-model.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <media-io/audio-resampler.h>
#include <util/platform.h>

#ifdef HINDSIGHT_HAS_WHISPER
#include <whisper.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <mutex>
#include <string_view>
#include <thread>

namespace {

#ifdef HINDSIGHT_HAS_WHISPER

// Whisper works on 16 kHz mono audio.
constexpr uint32_t kWhisperSampleRate = 16000;

// Resamples to 16 kHz with libobs's resampler. Empty on failure.
std::vector<float> to_whisper_rate(const std::vector<float> &samples, uint32_t sample_rate)
{
	if (sample_rate == kWhisperSampleRate)
		return samples;

	const resample_info from = {sample_rate, AUDIO_FORMAT_FLOAT, SPEAKERS_MONO};
	const resample_info to = {kWhisperSampleRate, AUDIO_FORMAT_FLOAT, SPEAKERS_MONO};
	audio_resampler_t *resampler = audio_resampler_create(&to, &from);
	if (!resampler)
		return {};

	// The resampler holds back a few samples; a little silence after the
	// clip pushes the last word through.
	std::vector<float> input = samples;
	input.resize(samples.size() + sample_rate / 10, 0.0f);

	std::vector<float> out;
	const auto *in = reinterpret_cast<const uint8_t *>(input.data());
	uint8_t *output[MAX_AV_PLANES] = {};
	uint32_t out_frames = 0;
	uint64_t offset = 0;
	if (audio_resampler_resample(resampler, output, &out_frames, &offset, &in,
				     static_cast<uint32_t>(input.size()))) {
		const auto *resampled = reinterpret_cast<const float *>(output[0]);
		out.assign(resampled, resampled + out_frames);
	}
	audio_resampler_destroy(resampler);
	return out;
}

// whisper.cpp is built for CPUs with AVX2, FMA and F16C (see whisper.cmake).
// Running it on an older CPU would crash OBS, so check first.
bool cpu_supported()
{
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
	return IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE) != 0;
#elif defined(__x86_64__) || defined(__i386__)
	__builtin_cpu_init();
	return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma") && __builtin_cpu_supports("f16c");
#else
	return true;
#endif
}

// whisper.cpp logs a lot; keep only its warnings and errors.
void on_whisper_log(ggml_log_level level, const char *text, void *)
{
	if (level == GGML_LOG_LEVEL_WARN || level == GGML_LOG_LEVEL_ERROR) {
		obs_log(LOG_WARNING, "whisper: %s", text);
		return;
	}
	// whisper_print_timings's breakdown of each transcription (fallbacks,
	// encode, decode), minus its load and total lines, which count from when
	// the model loaded rather than this clip.
	const std::string_view line = text ? text : "";
	constexpr std::string_view timings = "whisper_print_timings:";
	if (line.substr(0, timings.size()) == timings && line.find("load time") == std::string_view::npos &&
	    line.find("total time") == std::string_view::npos) {
		std::string_view detail = line.substr(timings.size());
		while (!detail.empty() && (detail.front() == ' ' || detail.back() == '\n'))
			detail = detail.front() == ' ' ? detail.substr(1) : detail.substr(0, detail.size() - 1);
		obs_log(LOG_INFO, "whisper %.*s", static_cast<int>(detail.size()), detail.data());
	}
}

// Set while the plugin unloads, so a transcription in progress stops instead
// of holding up closing OBS.
std::atomic<bool> unloading{false};

bool on_abort(void *data)
{
	return unloading.load() || static_cast<const std::atomic<bool> *>(data)->load();
}

// Lowers the calling thread's priority while it lives, so transcription
// yields to OBS's video and audio threads.
class LowPriority {
public:
	LowPriority()
	{
#ifdef _WIN32
		thread = GetCurrentThread();
		previous = GetThreadPriority(thread);
		SetThreadPriority(thread, THREAD_PRIORITY_BELOW_NORMAL);
#endif
	}
	~LowPriority()
	{
#ifdef _WIN32
		SetThreadPriority(thread, previous);
#endif
	}
	LowPriority(const LowPriority &) = delete;
	LowPriority &operator=(const LowPriority &) = delete;

private:
#ifdef _WIN32
	HANDLE thread = nullptr;
	int previous = THREAD_PRIORITY_NORMAL;
#endif
};

// One transcription at a time, and the loaded model, under model_lock.
std::mutex model_lock;
whisper_context *model = nullptr;

whisper_context *load_model(const std::string &path)
{
	whisper_log_set(on_whisper_log, nullptr);
	whisper_context_params params = whisper_context_default_params();
	params.use_gpu = false;
	const uint64_t start = os_gettime_ns();
	whisper_context *context = whisper_init_from_file_with_params(path.c_str(), params);
	if (context)
		obs_log(LOG_INFO, "speech model loaded in %.1f s", static_cast<double>(os_gettime_ns() - start) / 1e9);
	return context;
}

#endif

} // namespace

Transcript transcribe(const std::vector<float> &samples, uint32_t sample_rate, const std::atomic<bool> &cancel)
{
	Transcript result;
#ifndef HINDSIGHT_HAS_WHISPER
	(void)samples;
	(void)sample_rate;
	(void)cancel;
	result.outcome = Transcript::Outcome::unsupported;
	return result;
#else
	if (!cpu_supported()) {
		result.outcome = Transcript::Outcome::unsupported;
		return result;
	}
	std::unique_lock<std::mutex> guard(model_lock, std::try_to_lock);
	if (!guard.owns_lock()) {
		result.outcome = Transcript::Outcome::busy;
		return result;
	}
	if (unloading.load()) {
		result.outcome = Transcript::Outcome::cancelled;
		return result;
	}

	if (!model) {
		const std::string path = speech_model_path();
		if (path.empty()) {
			result.outcome = Transcript::Outcome::not_ready;
			return result;
		}
		// The file was checked when it downloaded, so a failure here is
		// likely passing (low memory); the next press tries again.
		model = load_model(path);
		if (!model) {
			obs_log(LOG_WARNING, "could not load the speech model from %s", path.c_str());
			return result;
		}
	}

	const std::vector<float> audio = to_whisper_rate(samples, sample_rate);
	if (audio.empty())
		return result;

	LowPriority low_priority;
	whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
	// Half the cores, at most four, leaves room for OBS.
	params.n_threads = static_cast<int>(std::clamp(std::thread::hardware_concurrency() / 2, 1u, 4u));
	params.language = "en";
	params.translate = false;
	params.no_context = true;
	params.no_timestamps = true;
	params.suppress_nst = true;
	params.print_progress = false;
	params.print_realtime = false;
	params.print_special = false;
	params.print_timestamps = false;
	params.abort_callback = on_abort;
	params.abort_callback_user_data = const_cast<std::atomic<bool> *>(&cancel);

	// Voice detection cuts the clip down to its speech, so Whisper neither
	// encodes nor second-guesses the silence and chatter around it.
	const std::string vad_path = vad_model_path();
	params.vad = !vad_path.empty();
	params.vad_model_path = params.vad ? vad_path.c_str() : nullptr;

	const uint64_t start = os_gettime_ns();
	whisper_reset_timings(model);
	int status = whisper_full(model, params, audio.data(), static_cast<int>(audio.size()));
	if (status != 0 && params.vad && !cancel.load() && !unloading.load()) {
		obs_log(LOG_WARNING, "voice detection failed; transcribing the whole clip instead");
		params.vad = false;
		params.vad_model_path = nullptr;
		status = whisper_full(model, params, audio.data(), static_cast<int>(audio.size()));
	}
	if (cancel.load() || unloading.load()) {
		result.outcome = Transcript::Outcome::cancelled;
		return result;
	}
	if (status != 0) {
		obs_log(LOG_WARNING, "transcription failed (whisper_full returned %d)", status);
		return result;
	}

	const int segments = whisper_full_n_segments(model);
	for (int i = 0; i < segments; i++) {
		const char *text = whisper_full_get_segment_text(model, i);
		if (text)
			result.text += text;
	}
	// Segments start with a space.
	const size_t first = result.text.find_first_not_of(' ');
	result.text.erase(0, first == std::string::npos ? result.text.size() : first);

	// Where the time went, to tell what to speed up next.
	double speech_seconds = static_cast<double>(audio.size()) / kWhisperSampleRate;
	if (params.vad) {
		speech_seconds = 0;
		const int spans = result.text.empty() ? 0 : whisper_full_n_vad_segments(model);
		for (int i = 0; i < spans; i++) // in centiseconds
			speech_seconds += static_cast<double>(whisper_full_get_vad_segment_t1(model, i) -
							      whisper_full_get_vad_segment_t0(model, i)) /
					  100.0;
	}
	obs_log(LOG_INFO, "transcribed %.0f s of audio (%.1f s of it speech%s) in %.1f s on %d threads",
		static_cast<double>(audio.size()) / kWhisperSampleRate, speech_seconds,
		params.vad ? "" : ", no voice detection", static_cast<double>(os_gettime_ns() - start) / 1e9,
		params.n_threads);
	whisper_print_timings(model);
	result.outcome = Transcript::Outcome::ok;
	return result;
#endif
}

void transcriber_unload()
{
#ifdef HINDSIGHT_HAS_WHISPER
	// Stops a transcription in progress, then waits for it to return.
	unloading = true;
	std::lock_guard<std::mutex> guard(model_lock);
	if (model)
		whisper_free(model);
	model = nullptr;
#endif
}
