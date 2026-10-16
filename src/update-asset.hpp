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
#include <string>
#include <string_view>

// Picks the right file out of a GitHub release for the Update now button.
// Free of OBS so the tests can run it on its own.

// The Windows installer's file name in a release, such as
// obs-hindsight-0.2.1-windows-x64-installer.exe. Empty when tag is not a
// version parse_release_version accepts, so the name is always safe to use as
// a file name.
std::string windows_installer_name(std::string_view tag);

// GitHub gives each release file a digest like "sha256:<64 hex digits>".
// Returns the 64 digits in lowercase, or nothing for any other form.
std::optional<std::string> sha256_from_digest(std::string_view digest);
