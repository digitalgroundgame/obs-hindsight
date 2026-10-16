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

#include <string>

// The Whisper model transcriber.cpp uses: base.en, about 142 MB, English only.
// It is not shipped with the plugin; the first time OBS starts with Hindsight,
// it downloads in the background into the plugin's config folder.

// Checks the saved model, or downloads it if it is missing or damaged, on a
// background thread. Called from obs_module_load, after curl_global_init.
void speech_model_start();

// Stops a download in progress. Called from obs_module_unload.
void speech_model_stop();

// The model file's path, or empty while it is missing or still downloading.
std::string speech_model_path();
