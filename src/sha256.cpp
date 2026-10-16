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

#include "sha256.hpp"

#ifdef _WIN32

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>

// https://learn.microsoft.com/en-us/windows/win32/seccng/creating-a-hash-with-cng
// Since Windows 7, BCryptCreateHash allocates the hash object itself when
// given no buffer.

Sha256::Sha256()
{
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
		failed_ = true;
		return;
	}
	algorithm_ = algorithm;
	if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0))) {
		failed_ = true;
		return;
	}
	hash_ = hash;
}

Sha256::~Sha256()
{
	if (hash_)
		BCryptDestroyHash(static_cast<BCRYPT_HASH_HANDLE>(hash_));
	if (algorithm_)
		BCryptCloseAlgorithmProvider(static_cast<BCRYPT_ALG_HANDLE>(algorithm_), 0);
}

void Sha256::add(const void *data, std::size_t size)
{
	auto *bytes = static_cast<PUCHAR>(const_cast<void *>(data));
	while (!failed_ && size > 0) {
		// BCryptHashData takes a ULONG length, so feed huge inputs in parts.
		const ULONG part = static_cast<ULONG>(std::min<std::size_t>(size, 1u << 30));
		if (!BCRYPT_SUCCESS(BCryptHashData(static_cast<BCRYPT_HASH_HANDLE>(hash_), bytes, part, 0)))
			failed_ = true;
		bytes += part;
		size -= part;
	}
}

std::string Sha256::finish()
{
	UCHAR digest[32];
	if (failed_ ||
	    !BCRYPT_SUCCESS(BCryptFinishHash(static_cast<BCRYPT_HASH_HANDLE>(hash_), digest, sizeof(digest), 0))) {
		failed_ = true;
		return {};
	}
	failed_ = true; // the hash object cannot be used again after finishing

	static const char kHex[] = "0123456789abcdef";
	std::string hex;
	hex.reserve(sizeof(digest) * 2);
	for (UCHAR byte : digest) {
		hex.push_back(kHex[byte >> 4]);
		hex.push_back(kHex[byte & 0x0f]);
	}
	return hex;
}

#endif
