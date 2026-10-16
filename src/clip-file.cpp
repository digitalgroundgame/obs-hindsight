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

#include "clip-file.hpp"

#include <cerrno>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// How many names to try before giving up: far more than anyone saves in one
// second.
constexpr int kMaxCopies = 999;

// Creates path for writing, failing if anything is already there. The "x"
// makes the check and the create a single step (C11 fopen; the UCRT has it
// too).
FILE *open_exclusive(const std::string &path)
{
#ifdef _WIN32
	// Windows' fopen reads paths in the ANSI code page, so widen the UTF-8
	// path to UTF-16 the way libobs's os_fopen does.
	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
					       static_cast<int>(path.size()), nullptr, 0);
	if (length <= 0) {
		errno = EINVAL;
		return nullptr;
	}
	std::wstring wide(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), wide.data(),
			    length);
	// _wfopen_s, not _wfopen, which MSVC warns is unsafe. It also keeps
	// other programs from opening the clip while it is being written.
	FILE *file = nullptr;
	const errno_t error = _wfopen_s(&file, wide.c_str(), L"wbx");
	if (error != 0) {
		errno = error;
		return nullptr;
	}
	return file;
#else
	return std::fopen(path.c_str(), "wbx");
#endif
}

} // namespace

NewFile create_new_file(const std::string &folder, const std::string &name, const std::string &extension)
{
	NewFile result;
	for (int copy = 1; copy <= kMaxCopies; copy++) {
		std::string path = folder;
		path += "/";
		path += name;
		if (copy > 1)
			path += "-" + std::to_string(copy);
		path += extension;

		errno = 0;
		FILE *file = open_exclusive(path);
		if (file) {
			result.file = file;
			result.path = std::move(path);
			return result;
		}
		// Only a taken name is worth another try; a missing folder or a
		// full disk fails the same way for every name.
		if (errno != EEXIST)
			break;
	}
	return result;
}
