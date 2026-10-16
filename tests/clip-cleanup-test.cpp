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
#include "clip-cleanup.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
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

// Makes an empty file written minutes_ago minutes before now.
void make_file(const fs::path &path, int minutes_ago)
{
	{
		std::ofstream out(path, std::ios::binary);
	}
	std::error_code error;
	fs::last_write_time(path, fs::file_time_type::clock::now() - std::chrono::minutes(minutes_ago), error);
	CHECK(!error);
}

void knows_saved_clip_names()
{
	CHECK(is_saved_clip_name("hindsight-20261016-230014.wav"));
	CHECK(is_saved_clip_name("hindsight-20261016-230014-2.wav"));
	CHECK(!is_saved_clip_name("hindsight-.wav"));
	CHECK(!is_saved_clip_name("hindsight-20261016-230014.wav.part"));
	CHECK(!is_saved_clip_name("ggml-base.en.bin"));
	CHECK(!is_saved_clip_name("my-notes.wav"));
}

void keeps_the_newest_clips(const fs::path &dir)
{
	make_file(dir / "hindsight-1.wav", 30);
	make_file(dir / "hindsight-2.wav", 20);
	make_file(dir / "hindsight-3.wav", 10);
	make_file(dir / "notes.txt", 40);

	CHECK(delete_old_clips(dir.u8string(), 2) == 1);
	CHECK(!fs::exists(dir / "hindsight-1.wav"));
	CHECK(fs::exists(dir / "hindsight-2.wav"));
	CHECK(fs::exists(dir / "hindsight-3.wav"));
	CHECK(fs::exists(dir / "notes.txt"));

	// Already within the limit: nothing to do.
	CHECK(delete_old_clips(dir.u8string(), 2) == 0);

	// keep = 0 deletes every clip, and still only clips.
	CHECK(delete_old_clips(dir.u8string(), 0) == 2);
	CHECK(!fs::exists(dir / "hindsight-2.wav"));
	CHECK(!fs::exists(dir / "hindsight-3.wav"));
	CHECK(fs::exists(dir / "notes.txt"));
}

void skips_folders_named_like_clips(const fs::path &dir)
{
	std::error_code error;
	fs::create_directory(dir / "hindsight-folder.wav", error);
	CHECK(!error);
	CHECK(delete_old_clips(dir.u8string(), 0) == 0);
	CHECK(fs::exists(dir / "hindsight-folder.wav"));
}

void handles_a_missing_folder(const fs::path &dir)
{
	CHECK(delete_old_clips((dir / "missing").u8string(), 0) == 0);
}

} // namespace

int main()
{
	knows_saved_clip_names();
	const fs::path dir = make_temp_dir();
	CHECK(!dir.empty());
	if (!dir.empty()) {
		keeps_the_newest_clips(dir);
		skips_folders_named_like_clips(dir);
		handles_a_missing_folder(dir);
		std::error_code error;
		fs::remove_all(dir, error);
		CHECK(!error);
	}
	return check::finish();
}
