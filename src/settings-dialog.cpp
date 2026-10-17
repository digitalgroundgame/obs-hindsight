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
#include "clip-cleanup.hpp"
#include "hindsight-filter.hpp"
#include "plugin-settings.hpp"
#include "secret-store.hpp"
#include "speech-model.hpp"
#include "update-check.hpp"
#include "update-notice.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QCheckBox>
#include <QDialog>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Where a streamer creates a Claude API key.
constexpr const char *kGetClaudeKeyUrl = "https://platform.claude.com/settings/keys";

#ifdef HINDSIGHT_ACCESS_CODES
// Where a streamer signs in with Discord to get their Hindsight code.
constexpr const char *kGetCodeUrl = "https://connect.digitalgroundgame.org";
#endif

QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

// How a status line looks. OBS's own themes (Yami and its light and classic
// variants, OBS 30 and later) colour labels by their "class" property, the way
// OBS's setClasses() sets it, so the marks follow the streamer's theme:
// https://github.com/obsproject/obs-studio/blob/31.1.1/frontend/data/themes/Yami.obt
// A theme without these classes still shows the mark, just uncoloured.
enum class Mark { good, busy, attention, bad };

const char *mark_glyph(Mark mark)
{
	switch (mark) {
	case Mark::good:
		return "✓"; // ✓
	case Mark::busy:
		return "…"; // …
	case Mark::attention:
		return "!";
	case Mark::bad:
		return "✕"; // ✕
	}
	return "";
}

const char *mark_class(Mark mark)
{
	switch (mark) {
	case Mark::good:
		return "text-success";
	case Mark::busy:
		return "text-muted";
	case Mark::attention:
		return "text-warning";
	case Mark::bad:
		return "text-danger";
	}
	return "";
}

// Sets a label's theme class. Qt only restyles a widget for a changed
// property when it is polished again, so this skips labels already showing it
// (the status timers call this several times a second).
void set_class(QLabel *label, const char *name)
{
	const QString wanted = QString::fromUtf8(name);
	if (label->property("class").toString() == wanted)
		return;
	label->setProperty("class", wanted);
	label->style()->unpolish(label);
	label->style()->polish(label);
}

// One line of the Status box: a coloured mark, what it means, and room for a
// button or two on the right.
struct StatusLine {
	QLabel *mark = nullptr;
	QLabel *label = nullptr;
	QHBoxLayout *buttons = nullptr;
};

StatusLine add_status_line(QGridLayout *grid, QWidget *parent)
{
	StatusLine line;
	const int row = grid->rowCount();
	line.mark = new QLabel(parent);
	QFont bold = line.mark->font();
	bold.setBold(true);
	line.mark->setFont(bold);
	line.mark->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	line.mark->setMinimumWidth(line.mark->fontMetrics().horizontalAdvance(QStringLiteral("✕")) + 4);
	line.label = new QLabel(parent);
	line.label->setWordWrap(true);
	line.buttons = new QHBoxLayout();
	grid->addWidget(line.mark, row, 0);
	grid->addWidget(line.label, row, 1);
	grid->addLayout(line.buttons, row, 2);
	return line;
}

void show_status(const StatusLine &line, Mark mark, const QString &message)
{
	line.mark->setText(QString::fromUtf8(mark_glyph(mark)));
	set_class(line.mark, mark_class(mark));
	line.label->setText(message);
}

// One key's row: a password box that starts empty (saved keys are never
// shown) and a button to forget the saved key. Its line in the Status box says
// whether one is saved.
struct KeyRow {
	QLineEdit *edit = nullptr;
	QPushButton *forget = nullptr;
	// Takes Forget's place while no key is saved, if given.
	QPushButton *get_key = nullptr;
	StatusLine status;
	const char *saved_text = nullptr;
	const char *missing_text = nullptr;
};

void show_saved_state(const KeyRow &row, bool saved)
{
	row.edit->setPlaceholderText(text(saved ? "Hindsight.KeySaved" : "Hindsight.KeyNotSet"));
	// Forget while a key is saved, the button to get one while it isn't.
	row.forget->setVisible(saved);
	if (row.get_key)
		row.get_key->setVisible(!saved);
	show_status(row.status, saved ? Mark::good : Mark::attention, text(saved ? row.saved_text : row.missing_text));
}

// extra, if given, goes beside the Forget button.
KeyRow add_key_row(QFormLayout *form, const char *label, QWidget *parent, QWidget *extra = nullptr)
{
	KeyRow row;
	row.edit = new QLineEdit(parent);
	row.edit->setEchoMode(QLineEdit::Password);
	row.forget = new QPushButton(text("Hindsight.Forget"), parent);

	auto *line = new QHBoxLayout();
	line->addWidget(row.edit, 1);
	line->addWidget(row.forget);
	if (extra)
		line->addWidget(extra);
	form->addRow(text(label), line);
	return row;
}

// The speech models' check or download, refreshed while the window is open.
struct ModelRow {
	StatusLine status;
	QProgressBar *progress = nullptr;
	QPushButton *retry = nullptr;
};

QString model_status_text(const SpeechModelStatus &status)
{
	const bool speech = status.model == SpeechModelStatus::Model::speech;
	switch (status.state) {
	case SpeechModelStatus::State::checking:
		return text("Hindsight.ModelChecking");
	case SpeechModelStatus::State::downloading:
		if (status.total <= 0)
			return text(speech ? "Hindsight.ModelDownloadingSpeech" : "Hindsight.ModelDownloadingVad");
		return text(speech ? "Hindsight.ModelDownloadingSpeechSize" : "Hindsight.ModelDownloadingVadSize")
			.arg(QString::number(static_cast<double>(status.downloaded) / 1e6, 'f', 0),
			     QString::number(static_cast<double>(status.total) / 1e6, 'f', 0));
	case SpeechModelStatus::State::ready:
		return text("Hindsight.ModelReady");
	case SpeechModelStatus::State::failed:
		return text(speech ? "Hindsight.ModelFailedSpeech" : "Hindsight.ModelFailedVad");
	}
	return {};
}

Mark model_mark(const SpeechModelStatus &status)
{
	switch (status.state) {
	case SpeechModelStatus::State::checking:
	case SpeechModelStatus::State::downloading:
		return Mark::busy;
	case SpeechModelStatus::State::ready:
		return Mark::good;
	case SpeechModelStatus::State::failed:
		// Without voice detection Hindsight still transcribes, only slower.
		return status.model == SpeechModelStatus::Model::speech ? Mark::bad : Mark::attention;
	}
	return Mark::busy;
}

void show_model_status(const ModelRow &row)
{
	const SpeechModelStatus status = speech_model_status();
	show_status(row.status, model_mark(status), model_status_text(status));
	row.retry->setVisible(status.state == SpeechModelStatus::State::failed);

	const bool busy = status.state == SpeechModelStatus::State::checking ||
			  status.state == SpeechModelStatus::State::downloading;
	row.progress->setVisible(busy);
	if (status.state == SpeechModelStatus::State::downloading && status.total > 0) {
		// In kilobytes, since QProgressBar counts in int.
		row.progress->setRange(0, static_cast<int>(status.total / 1024));
		row.progress->setValue(static_cast<int>(status.downloaded / 1024));
	} else {
		row.progress->setRange(0, 0); // a busy bar, for a check or an unknown size
	}
}

void add_model_row(QGridLayout *grid, QDialog *dialog)
{
	ModelRow row;
	row.status = add_status_line(grid, dialog);
	row.retry = new QPushButton(text("Hindsight.ModelRetry"), dialog);
	row.status.buttons->addWidget(row.retry);
	row.progress = new QProgressBar(dialog);
	row.progress->setTextVisible(false);
	grid->addWidget(row.progress, grid->rowCount(), 1, 1, 2);

	QObject::connect(row.retry, &QPushButton::clicked, dialog, [row]() {
		speech_model_start();
		show_model_status(row);
	});
	auto *timer = new QTimer(dialog);
	QObject::connect(timer, &QTimer::timeout, dialog, [row]() { show_model_status(row); });
	timer->start(250);
	show_model_status(row);
}

Mark update_mark(const UpdateNotice &notice)
{
	if (notice.is_error)
		return Mark::bad;
	if (notice.show)
		return Mark::attention;
	switch (update_check_state()) {
	case UpdateCheckState::up_to_date:
		return Mark::good;
	case UpdateCheckState::failed:
		return Mark::bad;
	case UpdateCheckState::not_checked:
	case UpdateCheckState::checking:
	case UpdateCheckState::newer:
		break;
	}
	return Mark::busy;
}

// The same update notice as the filter's settings, as the Status box's last
// line, or how the last check went with a Check for updates button. A timer
// redraws it while the window is open, so it follows a check or a download.
void add_update_line(QGridLayout *grid, QDialog *dialog)
{
	const StatusLine line = add_status_line(grid, dialog);
	auto *progress = new QProgressBar(dialog);
	progress->setRange(0, 100);
	grid->addWidget(progress, grid->rowCount(), 1, 1, 2);
	auto *update_now = new QPushButton(text("Hindsight.UpdateNow"), dialog);
	auto *release_page = new QPushButton(text("Hindsight.DownloadUpdate"), dialog);
	auto *check_now = new QPushButton(text("Hindsight.CheckNow"), dialog);
	line.buttons->addWidget(update_now);
	line.buttons->addWidget(release_page);
	line.buttons->addWidget(check_now);

	auto refresh = [line, progress, update_now, release_page, check_now]() {
		const UpdateNotice notice = update_notice_current();
		const std::string status = notice.show ? notice.text : update_check_status_text();
		show_status(line, update_mark(notice), QString::fromStdString(status));
		progress->setVisible(notice.percent >= 0);
		progress->setValue(notice.percent >= 0 ? notice.percent : 0);
		update_now->setVisible(notice.offer_update_now);
		update_now->setEnabled(notice.update_now_enabled);
		release_page->setVisible(notice.offer_release_page);
		check_now->setVisible(!notice.show);
		check_now->setEnabled(!update_check_running());
	};
	refresh();

	QObject::connect(update_now, &QPushButton::clicked, dialog, [refresh]() {
		update_notice_update_now(nullptr);
		refresh();
	});
	QObject::connect(release_page, &QPushButton::clicked, dialog,
			 []() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kHindsightReleasesPage))); });
	QObject::connect(check_now, &QPushButton::clicked, dialog, [refresh]() {
		update_check_start();
		refresh();
	});
	auto *timer = new QTimer(dialog);
	QObject::connect(timer, &QTimer::timeout, dialog, refresh);
	timer->start(500);
}

// A short grey line under a box's controls; the details are in its tooltip.
QLabel *add_note(QLayout *layout, const char *note, const char *details, QWidget *parent)
{
	auto *label = new QLabel(text(note), parent);
	label->setWordWrap(true);
	if (details)
		label->setToolTip(text(details));
	set_class(label, "text-muted");
	layout->addWidget(label);
	return label;
}

void open_settings_dialog(void *)
{
	auto *main_window = static_cast<QWidget *>(obs_frontend_get_main_window());
	QDialog dialog(main_window);
	dialog.setWindowTitle(text("Hindsight.SettingsTitle"));
	dialog.setMinimumWidth(520);

	auto *layout = new QVBoxLayout(&dialog);

	// Status: one line per thing Hindsight needs, each with its mark, so a
	// streamer can tell at a glance whether it is ready.
	auto *status_box = new QGroupBox(text("Hindsight.StatusTitle"), &dialog);
	auto *status = new QGridLayout(status_box);
	status->setColumnStretch(1, 1);
	layout->addWidget(status_box);

	// Keys: the boxes to type them in. Each key's line in Status comes first.
	auto *keys_box = new QGroupBox(text("Hindsight.KeysTitle"), &dialog);
	auto *keys = new QVBoxLayout(keys_box);
	auto *form = new QFormLayout();
	keys->addLayout(form);
	auto *get_claude_key = new QPushButton(text("Hindsight.GetClaudeKey"), &dialog);
	get_claude_key->setToolTip(text("Hindsight.GetClaudeKeyNote"));
	KeyRow claude = add_key_row(form, "Hindsight.ClaudeApiKey", &dialog, get_claude_key);
	claude.get_key = get_claude_key;
	claude.status = add_status_line(status, &dialog);
	claude.saved_text = "Hindsight.ClaudeKeySaved";
	claude.missing_text = "Hindsight.ClaudeKeyMissing";
	QObject::connect(get_claude_key, &QPushButton::clicked, &dialog,
			 []() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kGetClaudeKeyUrl))); });
#ifdef HINDSIGHT_ACCESS_CODES
	auto *get_code = new QPushButton(text("Hindsight.GetCode"), &dialog);
	KeyRow code = add_key_row(form, "Hindsight.HindsightCode", &dialog, get_code);
	code.get_key = get_code;
	code.status = add_status_line(status, &dialog);
	code.saved_text = "Hindsight.CodeSaved";
	code.missing_text = "Hindsight.CodeMissing";
	QObject::connect(get_code, &QPushButton::clicked, &dialog,
			 []() { QDesktopServices::openUrl(QUrl(QString::fromUtf8(kGetCodeUrl))); });
#else
	KeyRow discord = add_key_row(form, "Hindsight.DiscordBotToken", &dialog);
	discord.status = add_status_line(status, &dialog);
	discord.saved_text = "Hindsight.DiscordTokenSaved";
	discord.missing_text = "Hindsight.DiscordTokenMissing";
#endif
	add_note(keys, "Hindsight.SettingsNote", "Hindsight.SettingsNoteDetails", &dialog);
	layout->addWidget(keys_box);

	add_model_row(status, &dialog);
	add_update_line(status, &dialog);

	const Secrets saved = secret_store_get();
	show_saved_state(claude, !saved.claude_api_key.empty());
#ifdef HINDSIGHT_ACCESS_CODES
	show_saved_state(code, !saved.hindsight_code.empty());
#else
	show_saved_state(discord, !saved.discord_bot_token.empty());
#endif

	QObject::connect(claude.forget, &QPushButton::clicked, &dialog, [claude]() {
		if (secret_store_forget(SecretKey::ClaudeApiKey)) {
			claude.edit->clear();
			show_saved_state(claude, false);
		}
	});
#ifdef HINDSIGHT_ACCESS_CODES
	QObject::connect(code.forget, &QPushButton::clicked, &dialog, [code]() {
		if (secret_store_forget(SecretKey::HindsightCode)) {
			code.edit->clear();
			show_saved_state(code, false);
		}
	});
#else
	QObject::connect(discord.forget, &QPushButton::clicked, &dialog, [discord]() {
		if (secret_store_forget(SecretKey::DiscordBotToken)) {
			discord.edit->clear();
			show_saved_state(discord, false);
		}
	});
#endif

	// More: the update check setting and the saved clips.
	auto *more_box = new QGroupBox(text("Hindsight.MoreTitle"), &dialog);
	auto *more = new QVBoxLayout(more_box);
	layout->addWidget(more_box);

	// The update check reads this once when OBS starts, so a change here
	// takes effect at the next start.
	const bool was_checking = plugin_settings_check_for_updates();
	auto *check_updates = new QCheckBox(text("Hindsight.CheckForUpdates"), &dialog);
	check_updates->setChecked(was_checking);
	check_updates->setToolTip(text("Hindsight.CheckForUpdatesNote"));
	more->addWidget(check_updates);

	// Deleting clips happens at once, not on Save: it has nothing to undo.
	auto *clips_line = new QHBoxLayout();
	auto *clips_note = new QLabel(text("Hindsight.ClipsNote"), &dialog);
	clips_note->setWordWrap(true);
	auto *delete_clips = new QPushButton(text("Hindsight.DeleteClips"), &dialog);
	clips_line->addWidget(clips_note, 1);
	clips_line->addWidget(delete_clips);
	more->addLayout(clips_line);
	QObject::connect(delete_clips, &QPushButton::clicked, &dialog, [clips_note]() {
		const std::string folder = hindsight_clips_folder();
		const size_t deleted = folder.empty() ? 0 : delete_old_clips(folder, 0);
		obs_log(LOG_INFO, "deleted %zu saved clips", deleted);
		clips_note->setText(text("Hindsight.ClipsDeleted").arg(static_cast<qulonglong>(deleted)));
	});

	auto *error = new QLabel(&dialog);
	error->setWordWrap(true);
	set_class(error, "text-danger");
	error->hide();
	layout->addWidget(error);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	// Every widget lives until exec() returns, so capturing by reference is safe.
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
		Secrets typed;
		typed.claude_api_key = claude.edit->text().toStdString();
		const bool checking = check_updates->isChecked();

#ifdef HINDSIGHT_ACCESS_CODES
		typed.hindsight_code = code.edit->text().toStdString();
		const std::string typed_code = clean_key(typed.hindsight_code);
		if (!typed_code.empty() && !is_hindsight_code(typed_code)) {
			error->setText(text("Hindsight.BadCode"));
			error->show();
			return;
		}
#else
		typed.discord_bot_token = discord.edit->text().toStdString();
#endif

		// Blank boxes keep the saved key. On failure the window stays open
		// so nothing typed is lost.
		const bool setting_saved = checking == was_checking || plugin_settings_set_check_for_updates(checking);
		const bool keys_saved = secret_store_save_typed(typed);
		if (setting_saved && keys_saved) {
			dialog.accept();
		} else {
			error->setText(text("Hindsight.SaveFailed"));
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
