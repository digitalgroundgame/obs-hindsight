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

#include "secret-store.hpp"
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
	if (os_quick_write_utf8_file_safe(path.c_str(), contents.c_str(), contents.size(), false, "tmp", nullptr))
		return true;

	// libobs leaves the temporary file behind when the final replace fails
	// (say, antivirus has the file open). Don't leave the keys lying there.
	os_unlink((path + ".tmp").c_str());
	obs_log(LOG_ERROR, "could not save keys to %s", path.c_str());
	return false;
}

} // namespace

void secret_store_load()
{
	const std::string path = secrets_path();
	char *contents = path.empty() ? nullptr : os_quick_read_utf8_file(path.c_str());

	Secrets loaded;
	if (contents) {
		std::string text;
		if (open_secrets_text(contents, text))
			loaded = decode_secrets(text);
		else
			obs_log(LOG_WARNING, "could not read the saved keys; enter them again");
		bfree(contents);
	}

	std::lock_guard<std::mutex> guard(store_lock);
	store = loaded;
}

Secrets secret_store_get()
{
	std::lock_guard<std::mutex> guard(store_lock);
	return store;
}

bool secret_store_save_typed(const Secrets &typed)
{
	std::lock_guard<std::mutex> guard(store_lock);
	Secrets updated = store;
	if (!merge_typed_keys(updated, typed))
		return true;

	// Moving keys from an older build's filter settings can meet a different
	// key already saved (two filters, two keys). Say which one now applies.
	if (!store.claude_api_key.empty() && updated.claude_api_key != store.claude_api_key)
		obs_log(LOG_INFO, "replaced the saved Claude API key");
	if (!store.discord_bot_token.empty() && updated.discord_bot_token != store.discord_bot_token)
		obs_log(LOG_INFO, "replaced the saved Discord bot token");

	if (!write_secrets(updated))
		return false;
	store = updated;
	return true;
}

bool secret_store_forget(SecretKey key)
{
	std::lock_guard<std::mutex> guard(store_lock);
	Secrets updated = store;
	if (!forget_key(updated, key))
		return true;
	if (!write_secrets(updated))
		return false;
	store = updated;
	return true;
}
