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

#include "secrets.hpp"

// The plugin's one copy of the API keys, shared by every Hindsight filter and
// kept in a sealed file in the plugin's config folder (see secret-seal.hpp).
// All functions are safe to call from any thread.

// Reads the file. Called from obs_module_load. If the file exists but cannot
// be read (locked, or encrypted for another account), every later change
// first tries it again, so a Save never silently drops a key still in it.
void secret_store_load();

// A copy of the current keys.
Secrets secret_store_get();

// Saves any non-blank keys the streamer typed. Writes the file only when a
// key actually changes. Returns false if the file could not be written, in
// which case the stored keys are unchanged.
bool secret_store_save_typed(const Secrets &typed);

// Saves keys found in 0.1.0's filter settings, but only into empty slots
// (see fill_empty_keys). Returns false if the file could not be written, or
// if it exists but could not be read, so the old keys are kept for later.
bool secret_store_take_old_keys(const Secrets &old);

// Clears one key and saves. Returns false if the file could not be written.
bool secret_store_forget(SecretKey key);
