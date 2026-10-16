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

#include "secret-seal.hpp"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

#ifdef _WIN32

namespace {

constexpr const char *kScheme = "dpapi:";

bool apply_seal(const std::string &in, std::string &out, bool encrypt)
{
	DATA_BLOB input = {static_cast<DWORD>(in.size()), reinterpret_cast<BYTE *>(const_cast<char *>(in.data()))};
	DATA_BLOB output = {};
	const BOOL ok = encrypt ? CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
						   CRYPTPROTECT_UI_FORBIDDEN, &output)
				: CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
						     CRYPTPROTECT_UI_FORBIDDEN, &output);
	if (!ok)
		return false;
	out.assign(reinterpret_cast<const char *>(output.pbData), output.cbData);
	SecureZeroMemory(output.pbData, output.cbData);
	LocalFree(output.pbData);
	return true;
}

// Base64 from crypt32, the same Windows library as DPAPI.
bool to_base64(const std::string &bytes, std::string &text)
{
	const auto *data = reinterpret_cast<const BYTE *>(bytes.data());
	const auto size = static_cast<DWORD>(bytes.size());
	const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
	DWORD length = 0;
	if (!CryptBinaryToStringA(data, size, flags, nullptr, &length))
		return false;
	text.resize(length);
	if (!CryptBinaryToStringA(data, size, flags, text.data(), &length))
		return false;
	text.resize(length); // drops the terminating null
	return true;
}

bool from_base64(const std::string &text, std::string &bytes)
{
	const auto size = static_cast<DWORD>(text.size());
	DWORD length = 0;
	if (!CryptStringToBinaryA(text.data(), size, CRYPT_STRING_BASE64, nullptr, &length, nullptr, nullptr))
		return false;
	bytes.resize(length);
	if (!CryptStringToBinaryA(text.data(), size, CRYPT_STRING_BASE64, reinterpret_cast<BYTE *>(bytes.data()),
				  &length, nullptr, nullptr))
		return false;
	bytes.resize(length);
	return true;
}

} // namespace

std::string seal_secrets_text(const std::string &text)
{
	std::string sealed;
	std::string encoded;
	if (!apply_seal(text, sealed, true) || !to_base64(sealed, encoded))
		return "";
	return kScheme + encoded + "\n";
}

bool open_secrets_text(const std::string &contents, std::string &text)
{
	text.clear();
	const std::string scheme = kScheme;
	if (contents.compare(0, scheme.size(), scheme) != 0)
		return false;

	std::string encoded = contents.substr(scheme.size());
	while (!encoded.empty() && (encoded.back() == '\n' || encoded.back() == '\r'))
		encoded.pop_back();

	std::string sealed;
	return !encoded.empty() && from_base64(encoded, sealed) && apply_seal(sealed, text, false);
}

#else

namespace {

constexpr const char *kScheme = "plain:";

} // namespace

// Stubs until macOS (Keychain) and Linux (libsecret) encryption lands in #15:
// the text is stored as is, and secret-store.cpp makes the file readable by
// its owner only.
std::string seal_secrets_text(const std::string &text)
{
	return kScheme + text;
}

bool open_secrets_text(const std::string &contents, std::string &text)
{
	text.clear();
	const std::string scheme = kScheme;
	if (contents.compare(0, scheme.size(), scheme) != 0)
		return false;
	text = contents.substr(scheme.size());
	return true;
}

#endif
