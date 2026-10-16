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

#include "secret-store.hpp"
#include "private-file.hpp"
#include "secret-seal.hpp"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <mutex>
#include <string>

namespace {

constexpr const char *kSecretsFile = "secrets.dat";

std::mutex store_lock;
Secrets store;

// The sealed text is the keys as OBS settings JSON, the format OBS uses for
// its own config.
std::string encode_secrets(const Secrets &secrets)
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, kClaudeApiKeyName, clean_key(secrets.claude_api_key).c_str());
	obs_data_set_string(data, kHindsightCodeName, clean_key(secrets.hindsight_code).c_str());
	const char *json = obs_data_get_json(data);
	std::string text = json ? json : "";
	obs_data_release(data);
	return text;
}

// Sets has_bot_token when the file still holds a Discord bot token from
// before answers went through the DGG server.
bool decode_secrets(const std::string &text, Secrets &secrets, bool &has_bot_token)
{
	obs_data_t *data = obs_data_create_from_json(text.c_str());
	if (!data)
		return false;
	secrets.claude_api_key = clean_key(obs_data_get_string(data, kClaudeApiKeyName));
	secrets.hindsight_code = clean_key(obs_data_get_string(data, kHindsightCodeName));
	has_bot_token = obs_data_has_user_value(data, kDiscordBotTokenName);
	obs_data_release(data);
	return true;
}

std::string secrets_path()
{
	char *path = obs_module_config_path(kSecretsFile);
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

// Writes through a temporary file so a crash never leaves half a file, and
// keeps no backup copy so forgotten keys are really gone. Caller holds
// store_lock.
bool write_secrets(const Secrets &secrets)
{
	const std::string path = secrets_path();
	if (path.empty())
		return false;

	char *dir = obs_module_config_path("");
	if (dir)
		os_mkdirs(dir);
	bfree(dir);

	const std::string contents = seal_secrets_text(encode_secrets(secrets));
	if (contents.empty()) {
		obs_log(LOG_ERROR, "could not encrypt the keys");
		return false;
	}
	// libobs truncates this file in place and renames it, which keeps its
	// owner-only mode.
	if (!create_private_file(path + ".tmp")) {
		obs_log(LOG_ERROR, "could not create a private file to save the keys in");
		return false;
	}
	if (os_quick_write_utf8_file_safe(path.c_str(), contents.c_str(), contents.size(), false, "tmp", nullptr))
		return true;

	// libobs leaves the temporary file behind when the final replace fails
	// (say, antivirus has the file open). Don't leave the keys lying there.
	os_unlink((path + ".tmp").c_str());
	obs_log(LOG_ERROR, "could not save keys to %s", path.c_str());
	return false;
}

// Reads the keys file into secrets. Returns false, leaving secrets empty, if
// the file exists but could not be read: locked by antivirus or a sync tool,
// or encrypted for another Windows account or PC. A missing file is just no
// saved keys. See decode_secrets for has_bot_token.
bool read_secrets(Secrets &secrets, bool &has_bot_token)
{
	secrets = Secrets();
	has_bot_token = false;
	const std::string path = secrets_path();
	if (path.empty() || !os_file_exists(path.c_str()))
		return true;

	char *contents = os_quick_read_utf8_file(path.c_str());
	if (!contents)
		return false;
	std::string text;
	const bool ok = open_secrets_text(contents, text) && decode_secrets(text, secrets, has_bot_token);
	bfree(contents);
	if (!ok)
		secrets = Secrets();
	return ok;
}

// True while the keys file exists but has not been read, so the store may be
// missing keys that are still in the file.
bool file_unread = false;

// Called before every change to the keys, so a save never replaces a file
// whose keys were never loaded: tries the file again, since a lock may have
// cleared since OBS started. Caller holds store_lock.
void reread_if_needed()
{
	if (!file_unread)
		return;
	Secrets loaded;
	bool has_bot_token = false;
	if (read_secrets(loaded, has_bot_token)) {
		file_unread = false;
		store = loaded;
	}
}

// Writes the keys, first warning if that replaces a file that still could not
// be read and a key is blank, since that key is then lost. Caller holds
// store_lock.
bool replace_secrets(const Secrets &updated)
{
	if (file_unread && (updated.claude_api_key.empty() || updated.hindsight_code.empty()))
		obs_log(LOG_WARNING,
			"the saved keys still cannot be read and this Save replaces them; enter any blank key again");
	if (!write_secrets(updated))
		return false;
	file_unread = false;
	return true;
}

} // namespace

void secret_store_load()
{
	std::lock_guard<std::mutex> guard(store_lock);
	Secrets loaded;
	bool has_bot_token = false;
	file_unread = !read_secrets(loaded, has_bot_token);
	if (file_unread)
		obs_log(LOG_WARNING, "could not read the saved keys; Hindsight tries again before the next Save");
	store = loaded;

	// Answers now go through the DGG server's bot, so a bot token saved by an
	// earlier version is no longer needed. Rewriting the file drops it.
	if (has_bot_token) {
		if (write_secrets(store))
			obs_log(LOG_INFO, "erased the Discord bot token saved by an earlier version");
		else
			obs_log(LOG_WARNING, "could not erase the Discord bot token saved by an earlier version");
	}
}

Secrets secret_store_get()
{
	std::lock_guard<std::mutex> guard(store_lock);
	return store;
}

bool secret_store_save_typed(const Secrets &typed)
{
	std::lock_guard<std::mutex> guard(store_lock);
	reread_if_needed();
	Secrets updated = store;
	if (!merge_typed_keys(updated, typed))
		return true;

	if (!replace_secrets(updated))
		return false;
	store = updated;
	return true;
}

bool secret_store_take_old_keys(const Secrets &old)
{
	std::lock_guard<std::mutex> guard(store_lock);
	reread_if_needed();
	// Moving keys happens on its own as filters load, so never let it
	// replace a file that may hold newer keys; try again on the next load.
	if (file_unread)
		return false;
	Secrets updated = store;
	if (!fill_empty_keys(updated, old))
		return true;
	if (!replace_secrets(updated))
		return false;
	obs_log(LOG_INFO, "moved keys from the filter settings into Tools > Hindsight settings");
	store = updated;
	return true;
}

bool secret_store_forget(SecretKey key)
{
	std::lock_guard<std::mutex> guard(store_lock);
	reread_if_needed();
	Secrets updated = store;
	if (!forget_key(updated, key))
		return true;
	if (!replace_secrets(updated))
		return false;
	store = updated;
	return true;
}
