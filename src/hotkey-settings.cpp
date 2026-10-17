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

#include "hotkey-settings.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QAction>
#include <QApplication>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QStackedWidget>
#include <QString>
#include <QTimer>

namespace {

// How long to wait for OBS's Settings window, and then for its hotkeys to
// load, before giving up: 50 tries 100 ms apart.
constexpr int kRetryMs = 100;
constexpr int kMaxTries = 50;

// The object names below come from OBS 31's
// frontend/forms/OBSBasicSettings.ui and OBSBasic.ui.
QWidget *find_settings_window()
{
	for (QWidget *widget : QApplication::topLevelWidgets()) {
		if (widget->objectName() == "OBSBasicSettings" && widget->isVisible())
			return widget;
	}
	return nullptr;
}

// OBS builds the hotkey list only when the page is first shown, and empties
// the filter box as it does, so the search goes in once the list is there.
void fill_search(QPointer<QWidget> settings, QString search, int tries)
{
	if (!settings)
		return;
	auto *layout = settings->findChild<QFormLayout *>("hotkeyFormLayout");
	auto *box = settings->findChild<QLineEdit *>("hotkeyFilterSearch");
	if (!layout || !box) {
		obs_log(LOG_WARNING, "could not find the hotkey search box in OBS settings");
		return;
	}
	if (layout->rowCount() == 0) {
		if (tries < kMaxTries)
			QTimer::singleShot(kRetryMs, settings, [=] { fill_search(settings, search, tries + 1); });
		return;
	}
	// Setting the text runs OBS's own filter, as if the streamer typed it.
	box->setText(search);
}

void show_hotkeys_page(QString search, int tries)
{
	QWidget *settings = find_settings_window();
	if (!settings) {
		if (tries < kMaxTries)
			QTimer::singleShot(kRetryMs, qApp, [=] { show_hotkeys_page(search, tries + 1); });
		return;
	}

	// The list on the left and the pages on the right share their order.
	auto *list = settings->findChild<QListWidget *>("listWidget");
	auto *pages = settings->findChild<QStackedWidget *>("settingsPages");
	QWidget *hotkeys = settings->findChild<QWidget *>("hotkeyPage");
	const int row = pages && hotkeys ? pages->indexOf(hotkeys) : -1;
	if (!list || row < 0) {
		obs_log(LOG_WARNING, "could not find the Hotkeys page in OBS settings");
		return;
	}
	list->setCurrentRow(row);
	fill_search(settings, search, 0);
}

} // namespace

void open_hotkey_settings(const char *search)
{
	auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QAction *settings_action = main_window ? main_window->findChild<QAction *>("action_Settings") : nullptr;
	if (!settings_action) {
		obs_log(LOG_WARNING, "could not find OBS's Settings menu item");
		return;
	}

	const QString text = QString::fromUtf8(search);
	// OBS opens Settings with exec(), which does not return until the window
	// closes, so the trigger is queued to let this button click finish first,
	// and the page is picked from a timer that runs inside that window's loop.
	QTimer::singleShot(0, main_window, [=] {
		QTimer::singleShot(0, qApp, [=] { show_hotkeys_page(text, 0); });
		settings_action->trigger();
	});
}
