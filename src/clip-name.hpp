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

#pragma once

#include <ctime>
#include <string>

// The file name for a clip saved at `when`, such as
// "hindsight-20261015-231200.wav". `copy` numbers clips saved within the same
// second: 1 gives the plain name, 2 gives "hindsight-20261015-231200-2.wav",
// and so on. If the time cannot be turned into a date, the name uses the raw
// seconds instead. Kept free of OBS so the tests can run it on its own.
std::string clip_file_name(std::time_t when, int copy);
