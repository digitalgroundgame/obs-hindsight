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

#include "private-file.hpp"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef _WIN32

bool create_private_file(const std::string &)
{
	return true;
}

#else

bool create_private_file(const std::string &path)
{
	// O_NOFOLLOW: a symlink planted at path fails here instead of sending
	// the keys (and the chmod) to whatever it points at.
	const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return false;
	// open() leaves the mode of a file that already existed alone.
	const bool private_mode = fchmod(fd, S_IRUSR | S_IWUSR) == 0;
	return close(fd) == 0 && private_mode;
}

#endif
