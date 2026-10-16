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

#pragma once

#ifdef _WIN32

#include <cstddef>
#include <string>

// SHA-256 of data fed in pieces, using Windows' own CNG (bcrypt.dll), so a
// download can be checked as it arrives. Windows only: the Update now button
// that needs it only exists there.
class Sha256 {
public:
	Sha256();
	~Sha256();
	Sha256(const Sha256 &) = delete;
	Sha256 &operator=(const Sha256 &) = delete;

	void add(const void *data, std::size_t size);

	// The hash of everything added, as 64 lowercase hex digits. Empty if
	// Windows failed at any step. Call once, after the last add.
	std::string finish();

private:
	void *algorithm_ = nullptr; // BCRYPT_ALG_HANDLE
	void *hash_ = nullptr;      // BCRYPT_HASH_HANDLE
	bool failed_ = false;
};

#endif
