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

#include "check.hpp"
#include "local-time.hpp"

#include <cstdlib>
#include <ctime>
#include <string>

namespace {

std::tm make_tm(int year, int month, int day, int hour, int minute, int weekday)
{
	std::tm t{};
	t.tm_year = year - 1900;
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = minute;
	t.tm_wday = weekday;
	return t;
}

void writes_date_time_and_offset()
{
	CHECK(format_local_time(make_tm(2026, 10, 16, 2, 48, 5), -240) ==
	      "Friday, October 16, 2026, 2:48 AM (UTC-04:00)");
	CHECK(format_local_time(make_tm(2026, 1, 1, 23, 5, 4), 0) == "Thursday, January 1, 2026, 11:05 PM (UTC+00:00)");
	// India is UTC+5:30.
	CHECK(format_local_time(make_tm(2026, 7, 4, 12, 0, 6), 330) == "Saturday, July 4, 2026, 12:00 PM (UTC+05:30)");
	CHECK(format_local_time(make_tm(2026, 7, 4, 0, 30, 6), -150) == "Saturday, July 4, 2026, 12:30 AM (UTC-02:30)");
}

void rejects_out_of_range_fields()
{
	CHECK(format_local_time(make_tm(2026, 13, 1, 0, 0, 0), 0).empty());
	CHECK(format_local_time(make_tm(2026, 1, 1, 0, 0, 7), 0).empty());
}

void reads_the_clock_in_the_local_zone()
{
	// tests/CMakeLists.txt runs this test with TZ=EST5EDT, US Eastern time.
	// 2026-10-16 06:48:31 UTC, a Friday in daylight saving time.
	CHECK(local_time_text(1792133311) == "Friday, October 16, 2026, 2:48 AM (UTC-04:00)");
	// 2026-01-15 17:00:00 UTC, a Thursday in standard time.
	CHECK(local_time_text(1768496400) == "Thursday, January 15, 2026, 12:00 PM (UTC-05:00)");
	CHECK(!local_time_text().empty());
}

} // namespace

int main()
{
	writes_date_time_and_offset();
	rejects_out_of_range_fields();
	reads_the_clock_in_the_local_zone();
	return check::finish();
}
