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

#include "plugin-settings.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <string>

namespace {

constexpr const char *kSettingsFile = "settings.json";
constexpr const char *kCheckForUpdates = "check_for_updates";

std::string settings_path()
{
	char *path = obs_module_config_path(kSettingsFile);
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

// The saved settings, or empty ones if there is no file yet, with defaults
// filled in either way. Caller releases the result.
obs_data_t *read_settings(const std::string &path)
{
	obs_data_t *data = path.empty() ? nullptr : obs_data_create_from_json_file_safe(path.c_str(), "bak");
	if (!data)
		data = obs_data_create();
	obs_data_set_default_bool(data, kCheckForUpdates, true);
	return data;
}

} // namespace

bool plugin_settings_check_for_updates()
{
	obs_data_t *data = read_settings(settings_path());
	const bool on = obs_data_get_bool(data, kCheckForUpdates);
	obs_data_release(data);
	return on;
}

bool plugin_settings_set_check_for_updates(bool on)
{
	const std::string path = settings_path();
	if (path.empty())
		return false;

	char *dir = obs_module_config_path("");
	if (dir)
		os_mkdirs(dir);
	bfree(dir);

	// Start from the file so settings added later are kept.
	obs_data_t *data = read_settings(path);
	obs_data_set_bool(data, kCheckForUpdates, on);
	const bool saved = obs_data_save_json_safe(data, path.c_str(), "tmp", "bak");
	obs_data_release(data);
	if (!saved)
		obs_log(LOG_ERROR, "could not save settings to %s", path.c_str());
	return saved;
}
