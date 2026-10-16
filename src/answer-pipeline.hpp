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

#include <cstdint>
#include <vector>

// What the hotkey does with a clip: transcribe it on this PC, ask Claude for
// the question and answer, and write each step to the OBS log. Sending the
// answer to Discord comes later (#52). One clip at a time, on its own thread.

// Starts the worker. Called from obs_module_load.
void answer_pipeline_start();

// Stops a clip in progress and waits for the worker. Called from
// obs_module_unload, before transcriber_unload.
void answer_pipeline_stop();

// Hands a clip (mono float samples at sample_rate) to the worker. If it is
// still busy with the last one, logs that and drops this one.
void answer_pipeline_ask(std::vector<float> samples, uint32_t sample_rate);
