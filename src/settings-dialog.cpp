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
#include "secret-store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

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

KeyRow add_key_row(QFormLayout *form, const char *label, const char *forget_label, QWidget *parent)
{
	KeyRow row;
	row.edit = new QLineEdit(parent);
	row.edit->setEchoMode(QLineEdit::Password);
	row.forget = new QPushButton(QString::fromUtf8(obs_module_text(forget_label)), parent);

	auto *line = new QHBoxLayout();
	line->addWidget(row.edit, 1);
	line->addWidget(row.forget);
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
	const KeyRow claude = add_key_row(form, "Hindsight.ClaudeApiKey", "Hindsight.Forget", &dialog);
	const KeyRow discord = add_key_row(form, "Hindsight.DiscordBotToken", "Hindsight.Forget", &dialog);

	const Secrets saved = secret_store_get();
	show_saved_state(claude, !saved.claude_api_key.empty());
	show_saved_state(discord, !saved.discord_bot_token.empty());

	QObject::connect(claude.forget, &QPushButton::clicked, &dialog, [claude]() {
		if (secret_store_forget(SecretKey::ClaudeApiKey)) {
			claude.edit->clear();
			show_saved_state(claude, false);
		}
	});
	QObject::connect(discord.forget, &QPushButton::clicked, &dialog, [discord]() {
		if (secret_store_forget(SecretKey::DiscordBotToken)) {
			discord.edit->clear();
			show_saved_state(discord, false);
		}
	});

	auto *error = new QLabel(QString::fromUtf8(obs_module_text("Hindsight.SaveFailed")), &dialog);
	error->setWordWrap(true);
	error->hide();
	layout->addWidget(error);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, claude, discord, error]() {
		Secrets typed;
		typed.claude_api_key = claude.edit->text().toStdString();
		typed.discord_bot_token = discord.edit->text().toStdString();

		// Blank boxes keep the saved key. On failure the window stays open
		// so nothing typed is lost.
		if (secret_store_save_typed(typed))
			dialog.accept();
		else
			error->show();
	});

	dialog.exec();
}

} // namespace

void register_settings_dialog()
{
	obs_frontend_add_tools_menu_item(obs_module_text("Hindsight.SettingsMenu"), open_settings_dialog, nullptr);
}
