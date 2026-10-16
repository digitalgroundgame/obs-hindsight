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

// Whether file_name (no folder) is a clip Hindsight saved: hindsight-*.wav.
// Cleanup only ever touches these, so nothing else in the folder is lost.
bool is_saved_clip_name(const std::string &file_name);

// Deletes the saved clips in folder (UTF-8) except the newest keep of them,
// newest by modification time. keep = 0 deletes them all. A clip that can't
// be deleted (say, one still being written) is skipped. Returns how many were
// deleted. Free of OBS so the tests can run it on its own.
size_t delete_old_clips(const std::string &folder, size_t keep);
