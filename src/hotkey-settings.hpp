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

// Opens OBS's Settings window on the Hotkeys page with `search` typed into
// its filter box, so the streamer lands on the Hindsight hotkey. Call it on
// the UI thread (a property button's callback is).
//
// OBS has no API for this, so it presses OBS's own Settings menu item and
// then finds the Hotkeys page by the object names in OBS's settings form
// (frontend/forms/OBSBasicSettings.ui). If a future OBS renames them, the
// streamer still gets the Settings window, just not on the Hotkeys page.
void open_hotkey_settings(const char *search);
