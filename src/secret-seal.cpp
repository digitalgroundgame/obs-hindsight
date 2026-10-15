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

#include <QByteArray>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

#ifdef _WIN32
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
#else
constexpr const char *kScheme = "plain:";

bool apply_seal(const std::string &in, std::string &out, bool)
{
	out = in;
	return true;
}
#endif

// Base64 comes from Qt, which the plugin already links for its settings window.
std::string to_base64(const std::string &bytes)
{
	return QByteArray::fromRawData(bytes.data(), static_cast<qsizetype>(bytes.size())).toBase64().toStdString();
}

bool from_base64(const std::string &text, std::string &bytes)
{
	const auto decoded = QByteArray::fromBase64Encoding(
		QByteArray::fromRawData(text.data(), static_cast<qsizetype>(text.size())),
		QByteArray::AbortOnBase64DecodingErrors);
	if (!decoded)
		return false;
	bytes = decoded.decoded.toStdString();
	return true;
}

} // namespace

std::string seal_secrets_text(const std::string &text)
{
	std::string sealed;
	if (!apply_seal(text, sealed, true))
		return "";
	return kScheme + to_base64(sealed) + "\n";
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
	return from_base64(encoded, sealed) && apply_seal(sealed, text, false);
}
