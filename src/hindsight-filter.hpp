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

#include <cstddef>
#include <string>

// How many saved WAV clips to keep; saving one more deletes the oldest.
// Hindsight.ClipsNote in data/locale says this number too.
constexpr size_t kClipsToKeep = 10;

// Registers the "Hindsight" audio filter. Added to the streamer's mic, it
// keeps the last few seconds of that mic in memory and, on its hotkey, hands
// them to answer-pipeline.cpp, and saves them as a WAV file.
void register_hindsight_filter();

// The folder the filter saves clips in, created if missing, or "" if OBS
// cannot give the plugin a config folder.
std::string hindsight_clips_folder();
