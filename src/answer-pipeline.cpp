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

#include "answer-pipeline.hpp"
#include "claude-client.hpp"
#include "secret-store.hpp"
#include "transcriber.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {

struct Clip {
	std::vector<float> samples;
	uint32_t sample_rate = 0;
};

std::thread worker;
std::atomic<bool> stopping{false};

std::mutex lock;
std::condition_variable wake;
std::optional<Clip> waiting; // guarded by lock
bool busy = false;           // guarded by lock

double seconds_since(uint64_t start_ns)
{
	return static_cast<double>(os_gettime_ns() - start_ns) / 1e9;
}

const char *transcript_problem(Transcript::Outcome outcome)
{
	switch (outcome) {
	case Transcript::Outcome::not_ready:
		return "the speech model is still downloading (or failed to; see above). Try again in a minute";
	case Transcript::Outcome::unsupported:
		return "this PC can't run the transcriber (it needs a CPU with AVX2, on Windows or Linux)";
	case Transcript::Outcome::busy: // safety net; the pipeline runs one clip at a time
		return "still transcribing the last clip";
	case Transcript::Outcome::cancelled:
		return "cancelled";
	default:
		return "transcription failed; see the lines above";
	}
}

void answer(const Clip &clip)
{
	const uint64_t start = os_gettime_ns();
	obs_log(LOG_INFO, "answering: transcribing %.0f s of audio",
		static_cast<double>(clip.samples.size()) / clip.sample_rate);

	const Transcript transcript = transcribe(clip.samples, clip.sample_rate, stopping);
	if (transcript.outcome != Transcript::Outcome::ok) {
		obs_log(LOG_WARNING, "answering stopped: %s", transcript_problem(transcript.outcome));
		return;
	}
	if (transcript.text.empty()) {
		obs_log(LOG_INFO, "answering stopped: heard no speech");
		return;
	}
	obs_log(LOG_INFO, "heard (%.1f s): %s", seconds_since(start), transcript.text.c_str());

	const std::string api_key = secret_store_get().claude_api_key;
	if (api_key.empty()) {
		obs_log(LOG_WARNING, "answering stopped: add your Claude API key under Tools > Hindsight settings");
		return;
	}

	const uint64_t asked = os_gettime_ns();
	const ClaudeAnswer reply = ask_claude(transcript.text, api_key, stopping);
	switch (reply.outcome) {
	case ClaudeAnswer::Outcome::answered:
		obs_log(LOG_INFO, "question: %s", reply.question.c_str());
		obs_log(LOG_INFO,
			"answer (Claude took %.1f s, %.1f s in all; cost $%.5f for %lld in, %lld out tokens, %lld web searches): %s",
			seconds_since(asked), seconds_since(start),
			claude_cost_dollars(reply.input_tokens, reply.output_tokens, reply.web_searches),
			reply.input_tokens, reply.output_tokens, reply.web_searches, reply.answer.c_str());
		break;
	case ClaudeAnswer::Outcome::no_question:
		obs_log(LOG_INFO,
			"Claude found no clear question in that clip (%.1f s, cost $%.5f), so nothing to send",
			seconds_since(asked),
			claude_cost_dollars(reply.input_tokens, reply.output_tokens, reply.web_searches));
		break;
	case ClaudeAnswer::Outcome::paused: // ask_claude resumes these or turns them into failures
	case ClaudeAnswer::Outcome::failed:
		if (reply.error_type == "authentication_error")
			obs_log(LOG_WARNING,
				"answering stopped: Claude rejected the API key; check it in Tools > Hindsight settings");
		else if (reply.error_type == kWorkspaceKeyError)
			obs_log(LOG_WARNING,
				"answering stopped: this Claude API key isn't tied to a workspace. In the Claude Console, create a key with a workspace selected and enter it in Tools > Hindsight settings");
		else if (reply.error_type == kWebSearchDisabledError)
			obs_log(LOG_WARNING,
				"answering stopped: web search is turned off for your Claude organization. An admin can turn it on in the Claude Console under Settings > Capabilities");
		else if (reply.error_type != "cancelled")
			obs_log(LOG_WARNING, "answering stopped: the Claude request failed (%s) %s",
				reply.error_type.c_str(), reply.error_message.c_str());
		break;
	}
}

void run()
{
	for (;;) {
		Clip clip;
		{
			std::unique_lock<std::mutex> guard(lock);
			wake.wait(guard, [] { return stopping.load() || waiting.has_value(); });
			if (stopping.load())
				return;
			if (!waiting)
				continue;
			clip = std::move(*waiting);
			waiting.reset();
			busy = true;
		}
		answer(clip);
		std::lock_guard<std::mutex> guard(lock);
		busy = false;
	}
}

} // namespace

void answer_pipeline_start()
{
	if (worker.joinable())
		return;
	stopping = false;
	worker = std::thread(run);
}

void answer_pipeline_stop()
{
	{
		std::lock_guard<std::mutex> guard(lock);
		stopping = true;
	}
	wake.notify_all();
	if (worker.joinable())
		worker.join();
}

void answer_pipeline_ask(std::vector<float> samples, uint32_t sample_rate)
{
	{
		std::lock_guard<std::mutex> guard(lock);
		if (stopping.load())
			return;
		if (!worker.joinable()) {
			obs_log(LOG_WARNING,
				"can't answer: Hindsight didn't finish starting; see the log from OBS startup");
			return;
		}
		if (sample_rate == 0 || samples.empty())
			return;
		if (busy || waiting) {
			obs_log(LOG_WARNING,
				"still answering the last question; press the hotkey again when it's done");
			return;
		}
		waiting = Clip{std::move(samples), sample_rate};
	}
	wake.notify_one();
}
