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

#include <optional>
#include <string_view>

// A Hindsight version as buildspec.json and the release tags spell it:
// 1.2.3, 1.2.3-beta1 or 1.2.3-rc2. A leading "v" is accepted too.
struct ReleaseVersion {
	enum class Stage { beta, rc, release };

	int major = 0;
	int minor = 0;
	int patch = 0;
	Stage stage = Stage::release;
	int stage_number = 0; // the 1 in -beta1; 0 when there is none
};

// Parses text like "1.2.3" or "v1.2.3-rc1". Returns nothing for anything else,
// including other suffixes, so an unexpected tag is ignored rather than guessed at.
std::optional<ReleaseVersion> parse_release_version(std::string_view text);

// Whether a comes after b. For the same numbers, betas come first, then
// release candidates, then the release itself.
bool is_newer(const ReleaseVersion &a, const ReleaseVersion &b);

// Whether latest_tag names a newer release than running_version. False when
// either one does not parse, so a malformed tag never prompts an update.
bool update_available(std::string_view latest_tag, std::string_view running_version);
