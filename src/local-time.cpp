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

#include "local-time.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

const char *const kWeekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char *const kMonths[] = {"January", "February", "March",     "April",   "May",      "June",
			       "July",    "August",   "September", "October", "November", "December"};

// Days since 1970-01-01 for a date in the proleptic Gregorian calendar
// (Howard Hinnant's days_from_civil), so the UTC offset can be worked out
// without timegm, which Windows lacks.
long long days_from_civil(long long year, unsigned month, unsigned day)
{
	year -= month <= 2;
	const long long era = (year >= 0 ? year : year - 399) / 400;
	const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
	const unsigned day_of_year = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
	const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
	return era * 146097 + static_cast<long long>(day_of_era) - 719468;
}

} // namespace

std::string format_local_time(const std::tm &local, int utc_offset_minutes)
{
	if (local.tm_wday < 0 || local.tm_wday > 6 || local.tm_mon < 0 || local.tm_mon > 11)
		return {};
	const int hour12 = local.tm_hour % 12 == 0 ? 12 : local.tm_hour % 12;
	const int offset = std::abs(utc_offset_minutes);
	char text[96];
	std::snprintf(text, sizeof(text), "%s, %s %d, %d, %d:%02d %s (UTC%c%02d:%02d)", kWeekdays[local.tm_wday],
		      kMonths[local.tm_mon], local.tm_mday, local.tm_year + 1900, hour12, local.tm_min,
		      local.tm_hour < 12 ? "AM" : "PM", utc_offset_minutes < 0 ? '-' : '+', offset / 60, offset % 60);
	return text;
}

std::string local_time_text(std::time_t when)
{
	std::tm local{};
#ifdef _WIN32
	if (localtime_s(&local, &when) != 0)
		return {};
#else
	if (!localtime_r(&when, &local))
		return {};
#endif
	// The local fields read as if they were UTC, minus the real moment, is
	// the offset.
	const long long local_seconds = days_from_civil(local.tm_year + 1900LL, static_cast<unsigned>(local.tm_mon + 1),
							static_cast<unsigned>(local.tm_mday)) *
						86400 +
					local.tm_hour * 3600LL + local.tm_min * 60LL + local.tm_sec;
	const long long offset_seconds = local_seconds - static_cast<long long>(when);
	return format_local_time(local, static_cast<int>(offset_seconds / 60));
}
