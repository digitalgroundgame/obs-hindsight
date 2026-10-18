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

#include <string>

// Opening a new chat in the Claude desktop app (claude-app.hpp builds the
// link).

// Whether something on this PC opens claude:// links, which the Claude
// desktop app sets up when it's installed. Always true off Windows, where
// Hindsight doesn't check.
bool claude_app_installed();

// Opens the link, which brings up the Claude app's window. Returns false if
// the app isn't installed or Windows couldn't open it (logged). Blocks
// briefly, so call it off the audio thread; on Windows any thread will do.
bool claude_app_open(const std::string &link);
