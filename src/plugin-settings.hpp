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

// Hindsight's preferences that are not secrets, kept in settings.json in the
// plugin's config folder (separate from the keys file). Call these from
// OBS's UI thread.

// Whether to ask GitHub for a newer release when OBS starts. On unless the
// streamer turned it off; a missing or unreadable file counts as on.
bool plugin_settings_check_for_updates();

// Saves the choice. Returns false if the file could not be written.
bool plugin_settings_set_check_for_updates(bool on);
