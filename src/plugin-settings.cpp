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

#include <functional>
#include <mutex>
#include <string>

namespace {

constexpr const char *kSettingsFile = "settings.json";
constexpr const char *kCheckForUpdates = "check_for_updates";
constexpr const char *kAutoModel = "auto_model";
constexpr const char *kClaudeModelKey = "claude_model";
constexpr const char *kWebSearch = "web_search";
constexpr const char *kWebFetch = "web_fetch";
constexpr const char *kDiscordUserId = "discord_user_id";
constexpr const char *kUseOwnBot = "use_own_bot";
constexpr const char *kSendTo = "send_to";

// So a read never sees a save half done, and two saves don't undo each other.
std::mutex settings_lock;

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
	obs_data_set_default_bool(data, kAutoModel, true);
	obs_data_set_default_string(data, kClaudeModelKey, kDefaultClaudeModel.id);
	obs_data_set_default_bool(data, kWebSearch, true);
	obs_data_set_default_bool(data, kWebFetch, false);
	obs_data_set_default_bool(data, kUseOwnBot, false);
	obs_data_set_default_string(data, kSendTo, answer_destination_id(AnswerDestination::discord));
	return data;
}

// Reads the file, lets change set what it saves, and writes it back, so
// settings added later are kept. Hold settings_lock.
bool save_settings(const std::function<void(obs_data_t *)> &change)
{
	const std::string path = settings_path();
	if (path.empty())
		return false;

	char *dir = obs_module_config_path("");
	if (dir)
		os_mkdirs(dir);
	bfree(dir);

	obs_data_t *data = read_settings(path);
	change(data);
	const bool saved = obs_data_save_json_safe(data, path.c_str(), "tmp", "bak");
	obs_data_release(data);
	if (!saved)
		obs_log(LOG_ERROR, "could not save settings to %s", path.c_str());
	return saved;
}

} // namespace

bool plugin_settings_check_for_updates()
{
	std::lock_guard<std::mutex> guard(settings_lock);
	obs_data_t *data = read_settings(settings_path());
	const bool on = obs_data_get_bool(data, kCheckForUpdates);
	obs_data_release(data);
	return on;
}

bool plugin_settings_set_check_for_updates(bool on)
{
	std::lock_guard<std::mutex> guard(settings_lock);
	return save_settings([on](obs_data_t *data) { obs_data_set_bool(data, kCheckForUpdates, on); });
}

ClaudeOptions plugin_settings_claude_options()
{
	std::lock_guard<std::mutex> guard(settings_lock);
	obs_data_t *data = read_settings(settings_path());
	ClaudeOptions options;
	options.auto_model = obs_data_get_bool(data, kAutoModel);
	// A model this version doesn't offer reads as the default.
	options.model = claude_model(obs_data_get_string(data, kClaudeModelKey)).id;
	options.web_search = obs_data_get_bool(data, kWebSearch);
	options.web_fetch = obs_data_get_bool(data, kWebFetch);
	obs_data_release(data);
	return options;
}

bool plugin_settings_set_claude_options(const ClaudeOptions &options)
{
	std::lock_guard<std::mutex> guard(settings_lock);
	return save_settings([&options](obs_data_t *data) {
		obs_data_set_bool(data, kAutoModel, options.auto_model);
		obs_data_set_string(data, kClaudeModelKey, claude_model(options.model).id);
		obs_data_set_bool(data, kWebSearch, options.web_search);
		obs_data_set_bool(data, kWebFetch, options.web_fetch);
	});
}

std::string plugin_settings_discord_user_id()
{
	std::lock_guard<std::mutex> guard(settings_lock);
	obs_data_t *data = read_settings(settings_path());
	std::string user_id = obs_data_get_string(data, kDiscordUserId);
	obs_data_release(data);
	return user_id;
}

bool plugin_settings_use_own_bot()
{
	std::lock_guard<std::mutex> guard(settings_lock);
	obs_data_t *data = read_settings(settings_path());
	const bool on = obs_data_get_bool(data, kUseOwnBot);
	obs_data_release(data);
	return on;
}

bool plugin_settings_set_discord(const std::string &user_id, bool use_own_bot)
{
	std::lock_guard<std::mutex> guard(settings_lock);
	return save_settings([&user_id, use_own_bot](obs_data_t *data) {
		obs_data_set_string(data, kDiscordUserId, user_id.c_str());
		obs_data_set_bool(data, kUseOwnBot, use_own_bot);
	});
}

AnswerDestination plugin_settings_destination()
{
	std::lock_guard<std::mutex> guard(settings_lock);
	obs_data_t *data = read_settings(settings_path());
	const AnswerDestination destination = answer_destination_from_id(obs_data_get_string(data, kSendTo));
	obs_data_release(data);
	return destination;
}

bool plugin_settings_set_destination(AnswerDestination destination)
{
	std::lock_guard<std::mutex> guard(settings_lock);
	return save_settings([destination](obs_data_t *data) {
		obs_data_set_string(data, kSendTo, answer_destination_id(destination));
	});
}
