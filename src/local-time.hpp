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

#include <ctime>
#include <string>

// The streamer's local date and time, written out for the Claude prompt so
// Claude knows what "today" and "tonight" mean instead of answering "as of my
// last update".

// Writes a local time and its offset from UTC in minutes as, say,
// "Friday, October 16, 2026, 2:48 AM (UTC-04:00)". English on purpose: it is
// for Claude, not the streamer, and stays the same whatever the PC's locale.
std::string format_local_time(const std::tm &local, int utc_offset_minutes);

// format_local_time for the given moment (now by default) in this PC's time
// zone. Empty if the clock cannot be read.
std::string local_time_text(std::time_t when = std::time(nullptr));
