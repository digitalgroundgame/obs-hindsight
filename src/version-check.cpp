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

#include "version-check.hpp"

#include <cstddef>
#include <tuple>

namespace {

// Six digits is far beyond any real version and keeps int from overflowing.
constexpr std::size_t kMaxDigits = 6;

// Reads a run of digits from the front of text into value. Requires at least
// one digit when required is true; an empty run then yields 0.
bool take_number(std::string_view &text, int &value, bool required)
{
	std::size_t digits = 0;
	value = 0;
	while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9') {
		if (digits == kMaxDigits)
			return false;
		value = value * 10 + (text[digits] - '0');
		digits++;
	}
	if (required && digits == 0)
		return false;
	text.remove_prefix(digits);
	return true;
}

bool take_prefix(std::string_view &text, std::string_view prefix)
{
	if (text.substr(0, prefix.size()) != prefix)
		return false;
	text.remove_prefix(prefix.size());
	return true;
}

} // namespace

std::optional<ReleaseVersion> parse_release_version(std::string_view text)
{
	ReleaseVersion version;
	take_prefix(text, "v");

	if (!take_number(text, version.major, true) || !take_prefix(text, ".") ||
	    !take_number(text, version.minor, true) || !take_prefix(text, ".") ||
	    !take_number(text, version.patch, true))
		return std::nullopt;

	if (text.empty())
		return version;

	if (take_prefix(text, "-beta"))
		version.stage = ReleaseVersion::Stage::beta;
	else if (take_prefix(text, "-rc"))
		version.stage = ReleaseVersion::Stage::rc;
	else
		return std::nullopt;

	if (!take_number(text, version.stage_number, false) || !text.empty())
		return std::nullopt;
	return version;
}

bool is_newer(const ReleaseVersion &a, const ReleaseVersion &b)
{
	return std::tie(a.major, a.minor, a.patch, a.stage, a.stage_number) >
	       std::tie(b.major, b.minor, b.patch, b.stage, b.stage_number);
}

bool update_available(std::string_view latest_tag, std::string_view running_version)
{
	const std::optional<ReleaseVersion> latest = parse_release_version(latest_tag);
	const std::optional<ReleaseVersion> running = parse_release_version(running_version);
	return latest && running && is_newer(*latest, *running);
}

bool is_prerelease(std::string_view tag)
{
	const std::optional<ReleaseVersion> version = parse_release_version(tag);
	return version && version->stage != ReleaseVersion::Stage::release;
}

std::optional<std::size_t> newest_tag(const std::vector<std::string> &tags)
{
	std::optional<std::size_t> newest;
	std::optional<ReleaseVersion> newest_version;
	for (std::size_t i = 0; i < tags.size(); i++) {
		const std::optional<ReleaseVersion> version = parse_release_version(tags[i]);
		if (version && (!newest_version || is_newer(*version, *newest_version))) {
			newest = i;
			newest_version = version;
		}
	}
	return newest;
}
