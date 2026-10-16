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

#include "file-sha1.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <vector>
#endif

#ifdef _WIN32

namespace {

std::wstring widen(const std::string &utf8)
{
	const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
	if (length <= 0)
		return {};
	std::wstring wide(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), length);
	wide.resize(static_cast<size_t>(length) - 1); // drop the terminator
	return wide;
}

} // namespace

std::optional<std::string> file_sha1(const std::string &utf8_path)
{
	// Windows' fopen reads paths in the ANSI code page, so open by the UTF-16
	// path. _wfopen_s, not _wfopen, which MSVC warns is unsafe.
	FILE *file = nullptr;
	if (_wfopen_s(&file, widen(utf8_path).c_str(), L"rb") != 0 || !file)
		return std::nullopt;

	// BCRYPT_SHA1_ALG_HANDLE is a ready-made algorithm handle (Windows 10+).
	BCRYPT_HASH_HANDLE hash = nullptr;
	if (!BCRYPT_SUCCESS(BCryptCreateHash(BCRYPT_SHA1_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0))) {
		fclose(file);
		return std::nullopt;
	}

	bool ok = true;
	std::vector<unsigned char> buffer(1 << 20);
	for (;;) {
		const size_t read = fread(buffer.data(), 1, buffer.size(), file);
		if (read > 0 && !BCRYPT_SUCCESS(BCryptHashData(hash, buffer.data(), static_cast<ULONG>(read), 0))) {
			ok = false;
			break;
		}
		if (read < buffer.size()) {
			ok = ferror(file) == 0;
			break;
		}
	}
	fclose(file);

	unsigned char digest[20] = {};
	ok = ok && BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0));
	BCryptDestroyHash(hash);
	if (!ok)
		return std::nullopt;

	static const char hex[] = "0123456789abcdef";
	std::string result;
	for (unsigned char byte : digest) {
		result.push_back(hex[byte >> 4]);
		result.push_back(hex[byte & 0xf]);
	}
	return result;
}

#else

std::optional<std::string> file_sha1(const std::string &)
{
	return std::nullopt;
}

#endif
