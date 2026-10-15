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

#include "base64.hpp"

#include <cstdint>
#include <utility>

namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int value_of(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A';
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 26;
	if (c >= '0' && c <= '9')
		return c - '0' + 52;
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return -1;
}

} // namespace

std::string base64_encode(const std::string &bytes)
{
	std::string out;
	out.reserve((bytes.size() + 2) / 3 * 4);
	for (size_t i = 0; i < bytes.size(); i += 3) {
		const size_t left = bytes.size() - i;
		uint32_t chunk = static_cast<uint32_t>(static_cast<unsigned char>(bytes[i])) << 16;
		if (left > 1)
			chunk |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[i + 1])) << 8;
		if (left > 2)
			chunk |= static_cast<uint32_t>(static_cast<unsigned char>(bytes[i + 2]));

		out.push_back(kAlphabet[(chunk >> 18) & 63]);
		out.push_back(kAlphabet[(chunk >> 12) & 63]);
		out.push_back(left > 1 ? kAlphabet[(chunk >> 6) & 63] : '=');
		out.push_back(left > 2 ? kAlphabet[chunk & 63] : '=');
	}
	return out;
}

bool base64_decode(const std::string &text, std::string &out)
{
	out.clear();
	if (text.size() % 4 != 0)
		return false;

	std::string decoded;
	decoded.reserve(text.size() / 4 * 3);
	for (size_t i = 0; i < text.size(); i += 4) {
		const bool last = i + 4 == text.size();
		const int padding = (text[i + 3] == '=') + (text[i + 2] == '=' && text[i + 3] == '=');
		if (padding > 0 && !last)
			return false;

		uint32_t chunk = 0;
		for (int j = 0; j < 4; j++) {
			const char c = text[i + static_cast<size_t>(j)];
			int value = 0;
			if (j >= 4 - padding) {
				if (c != '=')
					return false;
			} else {
				value = value_of(c);
				if (value < 0)
					return false;
			}
			chunk = (chunk << 6) | static_cast<uint32_t>(value);
		}

		decoded.push_back(static_cast<char>((chunk >> 16) & 0xff));
		if (padding < 2)
			decoded.push_back(static_cast<char>((chunk >> 8) & 0xff));
		if (padding < 1)
			decoded.push_back(static_cast<char>(chunk & 0xff));
	}
	out = std::move(decoded);
	return true;
}
