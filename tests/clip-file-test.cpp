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
#include "clip-file.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

namespace {

// A fresh, empty folder for one test run; main removes it and its files.
fs::path make_temp_dir()
{
	std::error_code error;
	const fs::path base = fs::temp_directory_path(error);
	if (error)
		return {};
	std::random_device random;
	for (int attempt = 0; attempt < 100; attempt++) {
		const fs::path dir = base / ("hindsight-test-" + std::to_string(random()));
		if (fs::create_directory(dir, error))
			return dir;
	}
	return {};
}

std::string utf8(const fs::path &path)
{
	return path.u8string();
}

std::string contents_of(const fs::path &path)
{
	std::ifstream in(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Writes text to a file create_new_file made, and closes it.
void finish(NewFile &made, const char *text)
{
	CHECK(made.file != nullptr);
	if (made.file) {
		CHECK(std::fputs(text, made.file) >= 0);
		CHECK(std::fclose(made.file) == 0);
		made.file = nullptr;
	}
}

void creates_the_plain_name_first(const fs::path &dir)
{
	NewFile made = create_new_file(utf8(dir), "clip", ".wav");
	CHECK(made.path == utf8(dir) + "/clip.wav");
	finish(made, "first");
	CHECK(contents_of(dir / "clip.wav") == "first");
}

void numbers_the_name_when_it_is_taken(const fs::path &dir)
{
	// clip.wav is left from the test above.
	NewFile second = create_new_file(utf8(dir), "clip", ".wav");
	CHECK(second.path == utf8(dir) + "/clip-2.wav");
	finish(second, "second");

	NewFile third = create_new_file(utf8(dir), "clip", ".wav");
	CHECK(third.path == utf8(dir) + "/clip-3.wav");
	finish(third, "third");

	// Nothing that was already there was emptied or written over.
	CHECK(contents_of(dir / "clip.wav") == "first");
	CHECK(contents_of(dir / "clip-2.wav") == "second");
	CHECK(contents_of(dir / "clip-3.wav") == "third");
}

void never_truncates_an_existing_file(const fs::path &dir)
{
	{
		std::ofstream out(dir / "kept.wav", std::ios::binary);
		out << "already here";
	}
	NewFile made = create_new_file(utf8(dir), "kept", ".wav");
	CHECK(made.path == utf8(dir) + "/kept-2.wav");
	finish(made, "new");
	CHECK(contents_of(dir / "kept.wav") == "already here");
}

void open_files_get_different_names(const fs::path &dir)
{
	// Two writers that have both made their file and not yet closed it,
	// like two filters saving at the same moment.
	NewFile one = create_new_file(utf8(dir), "both", ".wav");
	NewFile two = create_new_file(utf8(dir), "both", ".wav");
	CHECK(one.path == utf8(dir) + "/both.wav");
	CHECK(two.path == utf8(dir) + "/both-2.wav");
	finish(one, "one");
	finish(two, "two");
	CHECK(contents_of(dir / "both.wav") == "one");
	CHECK(contents_of(dir / "both-2.wav") == "two");
}

void handles_utf8_names(const fs::path &dir)
{
	// "café" in UTF-8: on Windows this only works if the path is widened.
	const std::string name = "caf\xc3\xa9";
	NewFile made = create_new_file(utf8(dir), name, ".wav");
	CHECK(made.path == utf8(dir) + "/" + name + ".wav");
	finish(made, "accent");
	CHECK(contents_of(dir / fs::u8path(name + ".wav")) == "accent");
}

void fails_when_the_folder_is_missing(const fs::path &dir)
{
	NewFile made = create_new_file(utf8(dir / "missing"), "clip", ".wav");
	CHECK(made.file == nullptr);
	CHECK(made.path.empty());
	CHECK(!fs::exists(dir / "missing"));
}

} // namespace

int main()
{
	const fs::path dir = make_temp_dir();
	CHECK(!dir.empty());
	if (!dir.empty()) {
		creates_the_plain_name_first(dir);
		numbers_the_name_when_it_is_taken(dir);
		never_truncates_an_existing_file(dir);
		open_files_get_different_names(dir);
		handles_utf8_names(dir);
		fails_when_the_folder_is_missing(dir);
		std::error_code error;
		fs::remove_all(dir, error);
		CHECK(!error);
	}
	return check::finish();
}
