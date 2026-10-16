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

#pragma once

#include <string>

// Creates the file at path, or empties it if it exists, so that only its
// owner can read or write it, before anything secret goes in. The keys file
// needs this on macOS and Linux, where it is stored unencrypted until #15.
// Returns false if the file could not be created or its mode set. On Windows
// the keys are encrypted instead, so this does nothing and returns true.
// Free of OBS so the tests can run it on its own.
bool create_private_file(const std::string &path);
