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

#include "clip-cleanup.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

bool is_saved_clip_name(const std::string &file_name)
{
	const std::string prefix = "hindsight-";
	const std::string suffix = ".wav";
	return file_name.size() > prefix.size() + suffix.size() && file_name.compare(0, prefix.size(), prefix) == 0 &&
	       file_name.compare(file_name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

namespace {

size_t delete_old_clips_or_throw(const std::string &folder, size_t keep)
{
	std::error_code error;
	// u8path reads the UTF-8 folder correctly on Windows too.
	fs::directory_iterator entries(fs::u8path(folder), error);
	if (error)
		return 0;

	// Stepped with increment(error), not a range-for, whose ++ throws if
	// the folder changes mid-scan; this runs on threads that must not throw.
	std::vector<std::pair<fs::file_time_type, fs::path>> clips;
	for (; !error && entries != fs::directory_iterator(); entries.increment(error)) {
		const fs::directory_entry &entry = *entries;
		std::error_code entry_error;
		if (!entry.is_regular_file(entry_error) || !is_saved_clip_name(entry.path().filename().u8string()))
			continue;
		const fs::file_time_type written = entry.last_write_time(entry_error);
		if (!entry_error)
			clips.emplace_back(written, entry.path());
	}
	if (clips.size() <= keep)
		return 0;

	// Newest first; the name breaks ties, since it holds the time to the second.
	std::sort(clips.begin(), clips.end(), [](const auto &a, const auto &b) {
		return a.first != b.first ? a.first > b.first : a.second > b.second;
	});
	size_t deleted = 0;
	for (size_t i = keep; i < clips.size(); i++) {
		if (fs::remove(clips[i].second, error))
			deleted++;
	}
	return deleted;
}

} // namespace

size_t delete_old_clips(const std::string &folder, size_t keep)
{
	// Paths that can't be converted (or running out of memory) still throw;
	// cleanup is never worth crashing OBS over.
	try {
		return delete_old_clips_or_throw(folder, keep);
	} catch (const std::exception &) {
		return 0;
	}
}
