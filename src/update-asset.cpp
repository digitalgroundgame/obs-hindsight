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

#include "update-asset.hpp"
#include "version-check.hpp"

#include <cstddef>

std::string windows_installer_name(std::string_view tag)
{
	if (!parse_release_version(tag))
		return {};
	return "obs-hindsight-" + std::string(tag) + "-windows-x64-installer.exe";
}

std::optional<std::string> sha256_from_digest(std::string_view digest)
{
	constexpr std::string_view prefix = "sha256:";
	constexpr std::size_t kHexDigits = 64;
	if (digest.substr(0, prefix.size()) != prefix || digest.size() != prefix.size() + kHexDigits)
		return std::nullopt;

	std::string hex;
	hex.reserve(kHexDigits);
	for (char c : digest.substr(prefix.size())) {
		if (c >= 'A' && c <= 'F')
			c = static_cast<char>(c - 'A' + 'a');
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return std::nullopt;
		hex.push_back(c);
	}
	return hex;
}
