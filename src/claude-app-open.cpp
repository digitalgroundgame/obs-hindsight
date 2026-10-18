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

#include "claude-app-open.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#ifdef _WIN32
#include <util/platform.h>

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlwapi.h>
#else
#include <QDesktopServices>
#include <QString>
#include <QUrl>
#endif

#ifdef _WIN32

bool claude_app_installed()
{
	// Asks Windows which app opens claude:// links, the way a browser asks
	// before following one. ASSOCF_IS_PROTOCOL makes "claude" a link scheme
	// rather than a file extension, and covers Microsoft Store installs too.
	// The answer goes nowhere: asking for its length is enough to know there
	// is one.
	// https://learn.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-assocquerystringw
	DWORD length = 0;
	const HRESULT found =
		AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_FRIENDLYAPPNAME, L"claude", nullptr, nullptr, &length);
	return found == S_OK || found == S_FALSE;
}

bool claude_app_open(const std::string &link)
{
	// Without this check Windows would show its own "You'll need a new app"
	// dialog, which a streamer mid-game may not notice.
	if (!claude_app_installed()) {
		obs_log(LOG_WARNING,
			"not opened in the Claude app: it isn't installed. Get it from claude.com/download, or pick Discord under Tools > Hindsight settings");
		return false;
	}

	wchar_t *wide = nullptr;
	if (os_utf8_to_wcs_ptr(link.c_str(), link.size(), &wide) == 0 || !wide) {
		bfree(wide);
		obs_log(LOG_WARNING, "not opened in the Claude app: the question couldn't be converted");
		return false;
	}

	// The same as clicking the link: Windows hands it to the Claude app.
	// ShellExecuteEx may use COM, so the calling thread needs it set up:
	// https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw
	const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	SHELLEXECUTEINFOW info = {};
	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOASYNC;
	info.lpVerb = L"open";
	info.lpFile = wide;
	info.nShow = SW_SHOWNORMAL;
	const bool opened = ShellExecuteExW(&info) != FALSE;
	const DWORD error = opened ? 0 : GetLastError();

	if (SUCCEEDED(com))
		CoUninitialize();
	bfree(wide);
	if (!opened)
		obs_log(LOG_WARNING, "not opened in the Claude app: Windows couldn't open the link (error %lu)", error);
	return opened;
}

#else

bool claude_app_installed()
{
	return true;
}

namespace {

void open_on_ui_thread(void *data)
{
	auto *link = static_cast<std::string *>(data);
	if (!QDesktopServices::openUrl(QUrl(QString::fromStdString(*link), QUrl::StrictMode)))
		obs_log(LOG_WARNING, "not opened in the Claude app: nothing on this computer opens claude:// links");
	delete link;
}

} // namespace

bool claude_app_open(const std::string &link)
{
	// Qt opens links only from the UI thread.
	obs_queue_task(OBS_TASK_UI, open_on_ui_thread, new std::string(link), false);
	return true;
}

#endif
