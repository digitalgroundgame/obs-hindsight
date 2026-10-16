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

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

// Turns a clip of the streamer's mic into text with whisper.cpp, on their own
// PC: the audio never leaves it. Uses the model from speech-model.hpp.

struct Transcript {
	enum class Outcome {
		ok,          // text holds what was said (empty if nothing was)
		not_ready,   // the speech model is still downloading or failed to
		unsupported, // this CPU or OS can't run the transcriber
		busy,        // another clip is being transcribed
		cancelled,   // cancel was set
		failed,      // the model would not load or whisper.cpp failed
	};
	Outcome outcome = Outcome::failed;
	std::string text;
};

// Transcribes mono float samples at sample_rate. Takes a second or more, so
// call it off the audio and UI threads; it runs at low priority so a live
// stream doesn't drop frames. Setting cancel stops it early.
Transcript transcribe(const std::vector<float> &samples, uint32_t sample_rate, const std::atomic<bool> &cancel);

// Frees the loaded model. Called from obs_module_unload.
void transcriber_unload();
