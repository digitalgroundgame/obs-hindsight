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

#include "settings-dialog.hpp"
#include "plugin-settings.hpp"
#include "secret-store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Where a streamer signs in with Discord to get their Hindsight code.
constexpr const char *kGetCodeUrl = "https://stream-widgets.digitalgroundgame.org/hindsight";

// One key's row: a password box that starts empty (saved keys are never
// shown) and a button to forget the saved key.
struct KeyRow {
	QLineEdit *edit = nullptr;
	QPushButton *forget = nullptr;
};

void show_saved_state(const KeyRow &row, bool saved)
{
	row.edit->setPlaceholderText(
		QString::fromUtf8(obs_module_text(saved ? "Hindsight.KeySaved" : "Hindsight.KeyNotSet")));
	row.forget->setEnabled(saved);
}

// extra, if given, goes after the Forget button.
KeyRow add_key_row(QFormLayout *form, const char *label, QWidget *parent, QWidget *extra = nullptr)
{
	KeyRow row;
	row.edit = new QLineEdit(parent);
	row.edit->setEchoMode(QLineEdit::Password);
	row.forget = new QPushButton(QString::fromUtf8(obs_module_text("Hindsight.Forget")), parent);

	auto *line = new QHBoxLayout();
	line->addWidget(row.edit, 1);
	line->addWidget(row.forget);
	if (extra)
		line->addWidget(extra);
	form->addRow(QString::fromUtf8(obs_module_text(label)), line);
	return row;
}

void open_settings_dialog(void *)
{
	auto *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
	QDialog dialog(main_window);
	dialog.setWindowTitle(QString::fromUtf8(obs_module_text("Hindsight.SettingsTitle")));
	dialog.setMinimumWidth(480);

	auto *layout = new QVBoxLayout(&dialog);
	auto *note = new QLabel(QString::fromUtf8(obs_module_text("Hindsight.SettingsNote")), &dialog);
	note->setWordWrap(true);
	layout->addWidget(note);

	auto *form = new QFormLayout();
	layout->addLayout(form);
	const KeyRow claude = add_key_row(form, "Hindsight.ClaudeApiKey", &dialog);
	auto *get_code = new QPushButton(QString::fromUtf8(obs_module_text("Hindsight.GetCode")), &dialog);
	const KeyRow code = add_key_row(form, "Hindsight.HindsightCode", &dialog, get_code);
	QObject::connect(get_code, &QPushButton::clicked, &dialog,
			 []() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kGetCodeUrl))); });

	const Secrets saved = secret_store_get();
	show_saved_state(claude, !saved.claude_api_key.empty());
	show_saved_state(code, !saved.hindsight_code.empty());

	QObject::connect(claude.forget, &QPushButton::clicked, &dialog, [claude]() {
		if (secret_store_forget(SecretKey::ClaudeApiKey)) {
			claude.edit->clear();
			show_saved_state(claude, false);
		}
	});
	QObject::connect(code.forget, &QPushButton::clicked, &dialog, [code]() {
		if (secret_store_forget(SecretKey::HindsightCode)) {
			code.edit->clear();
			show_saved_state(code, false);
		}
	});

	// The update check reads this once when OBS starts, so a change here
	// takes effect at the next start.
	const bool was_checking = plugin_settings_check_for_updates();
	auto *check_updates = new QCheckBox(QString::fromUtf8(obs_module_text("Hindsight.CheckForUpdates")), &dialog);
	check_updates->setChecked(was_checking);
	layout->addWidget(check_updates);
	auto *check_updates_note =
		new QLabel(QString::fromUtf8(obs_module_text("Hindsight.CheckForUpdatesNote")), &dialog);
	check_updates_note->setWordWrap(true);
	layout->addWidget(check_updates_note);

	auto *error = new QLabel(&dialog);
	error->setWordWrap(true);
	error->hide();
	layout->addWidget(error);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	// Every widget lives until exec() returns, so capturing by reference is safe.
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
		Secrets typed;
		typed.claude_api_key = claude.edit->text().toStdString();
		typed.hindsight_code = code.edit->text().toStdString();
		const bool checking = check_updates->isChecked();

		const std::string typed_code = clean_key(typed.hindsight_code);
		if (!typed_code.empty() && !is_hindsight_code(typed_code)) {
			error->setText(QString::fromUtf8(obs_module_text("Hindsight.BadCode")));
			error->show();
			return;
		}

		// Blank boxes keep the saved key. On failure the window stays open
		// so nothing typed is lost.
		const bool setting_saved = checking == was_checking || plugin_settings_set_check_for_updates(checking);
		const bool keys_saved = secret_store_save_typed(typed);
		if (setting_saved && keys_saved) {
			dialog.accept();
		} else {
			error->setText(QString::fromUtf8(obs_module_text("Hindsight.SaveFailed")));
			error->show();
		}
	});

	dialog.exec();
}

} // namespace

void register_settings_dialog()
{
	obs_frontend_add_tools_menu_item(obs_module_text("Hindsight.SettingsMenu"), open_settings_dialog, nullptr);
}
