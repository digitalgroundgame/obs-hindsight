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
#include "private-file.hpp"

#ifndef _WIN32

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// A fresh, empty folder for one test run; main removes it with rmdir.
std::string make_temp_dir()
{
	const char *base = std::getenv("TMPDIR");
	std::string pattern = std::string(base && *base ? base : "/tmp") + "/hindsight-test-XXXXXX";
	return mkdtemp(pattern.data()) ? pattern : "";
}

mode_t mode_of(const std::string &path)
{
	struct stat info = {};
	return stat(path.c_str(), &info) == 0 ? (info.st_mode & 0777) : 0;
}

long size_of(const std::string &path)
{
	struct stat info = {};
	return stat(path.c_str(), &info) == 0 ? static_cast<long>(info.st_size) : -1;
}

void creates_an_owner_only_file(const std::string &dir)
{
	const std::string path = dir + "/new.tmp";
	CHECK(create_private_file(path));
	CHECK(mode_of(path) == 0600);
	CHECK(size_of(path) == 0);
	unlink(path.c_str());
}

void makes_an_existing_file_owner_only_and_empty(const std::string &dir)
{
	// A temporary file left behind by an older save, readable by everyone.
	const std::string path = dir + "/old.tmp";
	FILE *file = std::fopen(path.c_str(), "wb");
	CHECK(file != nullptr);
	if (file) {
		std::fputs("old keys", file);
		std::fclose(file);
	}
	CHECK(chmod(path.c_str(), 0644) == 0);

	CHECK(create_private_file(path));
	CHECK(mode_of(path) == 0600);
	CHECK(size_of(path) == 0);
	unlink(path.c_str());
}

void refuses_a_symlink(const std::string &dir)
{
	// A symlink at the temporary path must not redirect the keys elsewhere.
	const std::string target = dir + "/target";
	const std::string link = dir + "/link.tmp";
	FILE *file = std::fopen(target.c_str(), "wb");
	CHECK(file != nullptr);
	if (file) {
		std::fputs("not keys", file);
		std::fclose(file);
	}
	CHECK(chmod(target.c_str(), 0644) == 0);
	CHECK(symlink(target.c_str(), link.c_str()) == 0);

	CHECK(!create_private_file(link));
	CHECK(mode_of(target) == 0644);
	CHECK(size_of(target) == 8);
	unlink(link.c_str());
	unlink(target.c_str());
}

void fails_when_the_folder_is_missing(const std::string &dir)
{
	CHECK(!create_private_file(dir + "/missing/keys.tmp"));
}

} // namespace

int main()
{
	// The umask must not be what keeps the file private.
	umask(0);
	const std::string dir = make_temp_dir();
	CHECK(!dir.empty());
	if (!dir.empty()) {
		creates_an_owner_only_file(dir);
		makes_an_existing_file_owner_only_and_empty(dir);
		fails_when_the_folder_is_missing(dir);
		refuses_a_symlink(dir);
		rmdir(dir.c_str());
	}
	return check::finish();
}

#else

int main()
{
	// Windows encrypts the keys instead; the function only has to succeed.
	CHECK(create_private_file("unused"));
	return check::finish();
}

#endif
