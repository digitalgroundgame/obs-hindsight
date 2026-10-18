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
#include "claude-app-open.hpp"
#include "claude-app.hpp"
#include "claude-client.hpp"
#include "connect-client.hpp"
#include "discord-client.hpp"
#include "plugin-settings.hpp"
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

// DMs the answer to the streamer from their own Discord bot.
void send_with_own_bot(const ClaudeAnswer &reply, uint64_t start)
{
	const std::string token = secret_store_get().discord_bot_token;
	const std::string user_id = plugin_settings_discord_user_id();
	if (token.empty() || user_id.empty()) {
		obs_log(LOG_WARNING,
			"not sent to Discord: add your Discord bot token and user ID under Tools > Hindsight settings");
		return;
	}
	const uint64_t sending = os_gettime_ns();
	const DiscordReply sent =
		discord_send_dm(token, user_id, connect_message_content(reply.question, reply.answer), stopping);
	if (sent.ok)
		obs_log(LOG_INFO, "sent to your Discord DMs from your bot (%.1f s, %.1f s in all)",
			seconds_since(sending), seconds_since(start));
	else if (sent.error != "cancelled")
		obs_log(LOG_WARNING, "not sent to Discord (%s): %s", sent.error.c_str(), sent.message.c_str());
}

#ifdef HINDSIGHT_ACCESS_CODES
// DMs the answer to the streamer through Digital Ground Game Connect.
void send_with_code(const ClaudeAnswer &reply, uint64_t start)
{
	const std::string code = secret_store_get().hindsight_code;
	if (code.empty()) {
		obs_log(LOG_WARNING, "not sent to Discord: add your Hindsight code under Tools > Hindsight settings");
		return;
	}
	const uint64_t sending = os_gettime_ns();
	const ConnectReply sent =
		connect_send_message(code, connect_message_content(reply.question, reply.answer), stopping);
	if (sent.ok)
		obs_log(LOG_INFO, "sent to %s on Discord (%.1f s, %.1f s in all)", sent.discord_name.c_str(),
			seconds_since(sending), seconds_since(start));
	else if (sent.error != "cancelled")
		obs_log(LOG_WARNING, "not sent to Discord (%s): %s", sent.error.c_str(), sent.message.c_str());
}
#endif

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

	// The Claude app answers with the streamer's own Claude plan, so
	// Hindsight doesn't ask Claude itself.
	if (plugin_settings_destination() == AnswerDestination::claude_app) {
		if (claude_app_open(claude_app_link(transcript.text)))
			obs_log(LOG_INFO,
				"opened a new chat in the Claude app with that (%.1f s in all); send it there",
				seconds_since(start));
		return;
	}

	const std::string api_key = secret_store_get().claude_api_key;
	if (api_key.empty()) {
		obs_log(LOG_WARNING, "answering stopped: add your Claude API key under Tools > Hindsight settings");
		return;
	}

	// Read for each question, so a change in Hindsight settings applies to
	// the next one.
	const ClaudeOptions options = plugin_settings_claude_options();
	if (options.auto_model)
		obs_log(LOG_INFO, "asking Claude, model picked automatically, web search %s",
			options.web_search ? "on" : "off");
	else
		obs_log(LOG_INFO, "asking %s, web search %s", claude_model(options.model).id,
			options.web_search ? "on" : "off");

	const uint64_t asked = os_gettime_ns();
	const ClaudeAnswer reply = ask_claude(transcript.text, api_key, options, stopping);
	// Everything this question was billed for: picking the model, then the
	// answer from the model picked.
	const ClaudeModel &model = claude_model(reply.model);
	const double cost = reply.routing_dollars +
			    claude_cost_dollars(model, reply.input_tokens, reply.output_tokens, reply.web_searches);
	switch (reply.outcome) {
	case ClaudeAnswer::Outcome::answered:
		obs_log(LOG_INFO, "question: %s",
			reply.question.empty() ? "(Claude answered without restating it)" : reply.question.c_str());
		obs_log(LOG_INFO,
			"answer from %s (Claude took %.1f s, %.1f s in all; cost $%.5f for %lld in, %lld out tokens, %lld web searches): %s",
			model.id, seconds_since(asked), seconds_since(start), cost, reply.input_tokens,
			reply.output_tokens, reply.web_searches, reply.answer.c_str());
#ifdef HINDSIGHT_ACCESS_CODES
		if (!plugin_settings_use_own_bot()) {
			send_with_code(reply, start);
			break;
		}
#endif
		send_with_own_bot(reply, start);
		break;
	case ClaudeAnswer::Outcome::no_question:
		obs_log(LOG_INFO,
			"Claude found no clear question in that clip (%.1f s, cost $%.5f), so nothing to send",
			seconds_since(asked), cost);
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
				"answering stopped: web search or fetch is turned off for your Claude organization. An admin can turn it on in the Claude Console under Settings > Capabilities, or you can turn off Search the web or Fetch web pages in Tools > Hindsight settings");
		else if (reply.error_type != "cancelled")
			obs_log(LOG_WARNING, "answering stopped: the Claude request failed (%s) %s",
				reply.error_type.c_str(), reply.error_message.c_str());
		// A request that failed after Claude started (say, cut off after
		// searching) was still billed.
		if (reply.input_tokens > 0 || reply.output_tokens > 0 || reply.web_searches > 0 ||
		    reply.routing_dollars > 0)
			obs_log(LOG_INFO,
				"that request still cost $%.5f for %lld in, %lld out tokens, %lld web searches", cost,
				reply.input_tokens, reply.output_tokens, reply.web_searches);
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
