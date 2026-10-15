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

#include "clip-name.hpp"

namespace {

// localtime() can return null and is not thread-safe; these are the
// thread-safe versions each platform offers.
bool to_local_time(std::time_t when, std::tm &out)
{
#ifdef _WIN32
	return localtime_s(&out, &when) == 0;
#else
	return localtime_r(&when, &out) != nullptr;
#endif
}

} // namespace

std::string clip_file_name(std::time_t when, int copy)
{
	std::string stamp;
	std::tm local = {};
	char buffer[32];
	if (to_local_time(when, local) && std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &local) > 0)
		stamp = buffer;
	else
		stamp = std::to_string(static_cast<long long>(when));

	std::string name = "hindsight-" + stamp;
	if (copy > 1)
		name += "-" + std::to_string(copy);
	return name + ".wav";
}
