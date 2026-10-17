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

#include <obs-module.h>
#include <plugin-support.h>

#include "answer-pipeline.hpp"
#include "connect-client.hpp"
#include "hindsight-filter.hpp"
#include "plugin-settings.hpp"
#include "secret-store.hpp"
#include "settings-dialog.hpp"
#include "speech-model.hpp"
#include "transcriber.hpp"
#include "update-check.hpp"
#include "update-install.hpp"

#include <curl/curl.h>

namespace {
bool curl_ready = false;
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

bool obs_module_load(void)
{
	secret_store_load();
	register_hindsight_filter();
	register_settings_dialog();
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);

	// curl_global_init is not safe to race with other threads, so it runs
	// here on the thread that loads the module, before any worker starts.
	curl_ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
	if (!curl_ready) {
		obs_log(LOG_WARNING, "curl failed to start; no update check or speech model download");
		return true;
	}
	speech_model_start();
	answer_pipeline_start();
	if (plugin_settings_check_for_updates())
		update_check_start();
	else
		obs_log(LOG_INFO, "update check is turned off in Hindsight settings");
	return true;
}

void obs_module_unload(void)
{
	update_install_stop();
	update_check_stop();
	connect_test_stop();
	speech_model_stop();
	answer_pipeline_stop();
	transcriber_unload();
	if (curl_ready)
		curl_global_cleanup();
	curl_ready = false;
	obs_log(LOG_INFO, "plugin unloaded");
}
