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
#include "file-sha1.hpp"

#include <cstdio>
#include <string>

namespace {

std::string write_temp(const char *name, const std::string &contents)
{
#ifdef _WIN32
	FILE *file = nullptr;
	if (fopen_s(&file, name, "wb") != 0 || !file)
		return {};
#else
	FILE *file = std::fopen(name, "wb");
	if (!file)
		return {};
#endif
	std::fwrite(contents.data(), 1, contents.size(), file);
	std::fclose(file);
	return name;
}

void hashes_files()
{
	const std::string abc = write_temp("sha1-abc.tmp", "abc");
	const std::string empty = write_temp("sha1-empty.tmp", "");
	// A file bigger than one read, to cover the loop.
	const std::string big = write_temp("sha1-big.tmp", std::string(3 * 1024 * 1024 + 7, 'a'));

#ifdef _WIN32
	// Test vectors from FIPS 180.
	CHECK(file_sha1(abc) == "a9993e364706816aba3e25717850c26c9cd0d89d");
	CHECK(file_sha1(empty) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
	CHECK(file_sha1(big).has_value());
	CHECK(!file_sha1("no-such-file.tmp").has_value());
#else
	// Not implemented here yet.
	CHECK(!file_sha1(abc).has_value());
#endif

	std::remove(abc.c_str());
	std::remove(empty.c_str());
	std::remove(big.c_str());
}

} // namespace

int main()
{
	hashes_files();
	return check::finish();
}
