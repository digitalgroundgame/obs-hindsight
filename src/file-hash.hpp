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

// The SHA-1 or SHA-256 of a file as lowercase hex, used to check a download
// against the hash its publisher lists. Windows only for now (they use the
// system's BCrypt); elsewhere, and if the file can't be read, they return
// nothing.
std::optional<std::string> file_sha1(const std::string &utf8_path);
std::optional<std::string> file_sha256(const std::string &utf8_path);
