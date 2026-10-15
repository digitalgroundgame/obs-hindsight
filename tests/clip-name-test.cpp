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

#include "check.hpp"
#include "clip-name.hpp"

#include <cstdlib>
#include <ctime>
#include <string>

namespace {

void use_utc()
{
#ifdef _WIN32
	_putenv_s("TZ", "UTC");
	_tzset();
#else
	setenv("TZ", "UTC", 1);
	tzset();
#endif
}

void names_a_clip_by_its_local_time()
{
	CHECK(clip_file_name(0, 1) == "hindsight-19700101-000000.wav");
	CHECK(clip_file_name(1760570000, 1) == "hindsight-20251015-231320.wav");
}

void numbers_clips_in_the_same_second()
{
	CHECK(clip_file_name(0, 2) == "hindsight-19700101-000000-2.wav");
	CHECK(clip_file_name(0, 10) == "hindsight-19700101-000000-10.wav");
	// Anything below 2 is the first clip of that second.
	CHECK(clip_file_name(0, 0) == "hindsight-19700101-000000.wav");
}

} // namespace

int main()
{
	use_utc();
	names_a_clip_by_its_local_time();
	numbers_clips_in_the_same_second();
	return check::finish();
}
