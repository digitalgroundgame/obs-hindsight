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
#include "connect-client.hpp"
#include "discord-api.hpp"
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
#include <QComboBox>
#include <QDialog>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

namespace {

// Where a streamer creates a Claude API key.
constexpr const char *kGetClaudeKeyUrl = "https://platform.claude.com/settings/keys";

// Digital Ground Game's website, linked from the window's footer.
constexpr const char *kDggUrl = "https://digitalgroundgame.org";

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

// Shows or hides a whole status line, its buttons included.
void set_status_visible(const StatusLine &line, bool visible)
{
	line.mark->setVisible(visible);
	line.label->setVisible(visible);
	for (int i = 0; i < line.buttons->count(); i++)
		if (QWidget *button = line.buttons->itemAt(i)->widget())
			button->setVisible(visible);
}

// One key's row: a password box that starts empty (saved keys are never
// shown) and a button to forget the saved key. Its line in the Status box says
// whether one is saved.
struct KeyRow {
	QLineEdit *edit = nullptr;
	QPushButton *forget = nullptr;
	// Takes Forget's place while no key is saved, if given.
	QPushButton *get_key = nullptr;
	QHBoxLayout *line = nullptr; // the row's field, for hiding the row
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

	row.line = new QHBoxLayout();
	row.line->addWidget(row.edit, 1);
	row.line->addWidget(row.forget);
	if (extra)
		row.line->addWidget(extra);
	form->addRow(text(label), row.line);
	return row;
}

#ifdef HINDSIGHT_ACCESS_CODES
// A Test button on the code's status line. It sends a test DM with the code
// in the box, or the saved one, and shows how that went on the same line until
// the code is saved or forgotten.
void add_test_button(const KeyRow &code, QDialog *dialog)
{
	auto *test = new QPushButton(text("Hindsight.TestCode"), dialog);
	test->setToolTip(text("Hindsight.TestCodeNote"));
	code.status.buttons->addWidget(test);

	// Whether this window is waiting on a test it started, so a result left
	// from an earlier window isn't shown here.
	auto watching = std::make_shared<bool>(false);
	auto *timer = new QTimer(dialog);
	QObject::connect(timer, &QTimer::timeout, dialog, [code, test, watching, timer]() {
		const ConnectTestStatus status = connect_test_status();
		if (status.state == ConnectTestState::testing)
			return;
		timer->stop();
		test->setEnabled(true);
		if (!*watching)
			return;
		*watching = false;
		if (status.reply.ok)
			show_status(code.status, Mark::good,
				    text("Hindsight.TestSent").arg(QString::fromStdString(status.reply.discord_name)));
		else
			show_status(code.status, Mark::bad, QString::fromStdString(status.reply.message));
	});
	QObject::connect(test, &QPushButton::clicked, dialog, [code, test, watching, timer]() {
		std::string typed = clean_key(code.edit->text().toStdString());
		if (typed.empty())
			typed = secret_store_get().hindsight_code;
		if (typed.empty()) {
			show_status(code.status, Mark::attention, text("Hindsight.TestNeedsCode"));
			return;
		}
		if (!is_hindsight_code(typed)) {
			show_status(code.status, Mark::bad, text("Hindsight.BadCode"));
			return;
		}
		if (!connect_test_start(typed)) {
			show_status(code.status, Mark::busy, text("Hindsight.TestBusy"));
			return;
		}
		*watching = true;
		test->setEnabled(false);
		show_status(code.status, Mark::busy, text("Hindsight.Testing"));
		timer->start(250);
	});
}
#endif

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
	// One line only; the Update now tooltip has the details.
	line.label->setWordWrap(false);
	auto *progress = new QProgressBar(dialog);
	progress->setRange(0, 100);
	grid->addWidget(progress, grid->rowCount(), 1, 1, 2);
	auto *update_now = new QPushButton(text("Hindsight.UpdateNow"), dialog);
	update_now->setToolTip(text("Hindsight.UpdateNowTip"));
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

// The DGG logo and a link to the website, above the Save button.
void add_footer(QVBoxLayout *layout, QWidget *parent)
{
	auto *footer = new QHBoxLayout();
	char *logo_path = obs_module_file("dgg-logo.png");
	if (logo_path) {
		// Drawn at 32 pixels, sharp on high-DPI screens too.
		const qreal ratio = parent->devicePixelRatioF();
		QPixmap logo(QString::fromUtf8(logo_path));
		bfree(logo_path);
		if (!logo.isNull()) {
			logo = logo.scaled(QSize(32, 32) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			logo.setDevicePixelRatio(ratio);
			auto *logo_label = new QLabel(parent);
			logo_label->setPixmap(logo);
			footer->addWidget(logo_label);
		}
	}
	auto *link = new QLabel(text("Hindsight.MadeBy").arg(QString::fromUtf8(kDggUrl)), parent);
	link->setOpenExternalLinks(true);
	footer->addWidget(link, 1);
	layout->addLayout(footer);
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
	add_test_button(code, &dialog);
#endif
	// The streamer's own Discord bot: the only way to get answers in builds
	// without access codes, and the alternative to a code in builds with them.
	KeyRow discord = add_key_row(form, "Hindsight.DiscordBotToken", &dialog);
	discord.status = add_status_line(status, &dialog);
	discord.saved_text = "Hindsight.DiscordTokenSaved";
	discord.missing_text = "Hindsight.DiscordTokenMissing";
	// Not a secret, so it's shown and edited in place.
	auto *user_id = new QLineEdit(QString::fromStdString(plugin_settings_discord_user_id()), &dialog);
	user_id->setPlaceholderText(text("Hindsight.DiscordUserIdPlaceholder"));
	user_id->setToolTip(text("Hindsight.DiscordUserIdNote"));
	form->addRow(text("Hindsight.DiscordUserId"), user_id);
	add_note(keys, "Hindsight.SettingsNote", "Hindsight.SettingsNoteDetails", &dialog);

#ifdef HINDSIGHT_ACCESS_CODES
	// The code is the default. A small link swaps the code's row for the
	// bot's, and back; Save or Apply keeps the choice.
	auto own_bot = std::make_shared<bool>(plugin_settings_use_own_bot());
	auto *switch_link = new QLabel(&dialog);
	set_class(switch_link, "text-muted");
	keys->addWidget(switch_link);
	auto show_delivery = [form, code, discord, user_id, switch_link, own_bot]() {
		const bool bot = *own_bot;
		// QFormLayout shows or hides every widget in a row, so the shown
		// row's Forget and Get buttons are then set again.
		form->setRowVisible(code.line, !bot);
		form->setRowVisible(discord.line, bot);
		form->setRowVisible(user_id, bot);
		const Secrets saved = secret_store_get();
		if (bot)
			show_saved_state(discord, !saved.discord_bot_token.empty());
		else
			show_saved_state(code, !saved.hindsight_code.empty());
		set_status_visible(code.status, !bot);
		set_status_visible(discord.status, bot);
		switch_link->setText(
			QStringLiteral("<a href='#'>%1</a>")
				.arg(text(bot ? "Hindsight.UseCode" : "Hindsight.UseOwnBot").toHtmlEscaped()));
	};
	QObject::connect(switch_link, &QLabel::linkActivated, &dialog, [own_bot, show_delivery]() {
		*own_bot = !*own_bot;
		show_delivery();
	});
#endif
	layout->addWidget(keys_box);

	add_model_row(status, &dialog);
	add_update_line(status, &dialog);

	const Secrets saved = secret_store_get();
	show_saved_state(claude, !saved.claude_api_key.empty());
#ifdef HINDSIGHT_ACCESS_CODES
	show_saved_state(code, !saved.hindsight_code.empty());
#endif
	show_saved_state(discord, !saved.discord_bot_token.empty());
#ifdef HINDSIGHT_ACCESS_CODES
	show_delivery();
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
#endif
	QObject::connect(discord.forget, &QPushButton::clicked, &dialog, [discord]() {
		if (secret_store_forget(SecretKey::DiscordBotToken)) {
			discord.edit->clear();
			show_saved_state(discord, false);
		}
	});

	// Claude: which model answers, and whether it may search the web. Saved
	// with Save or Apply; the next question uses them.
	auto *claude_box = new QGroupBox(text("Hindsight.ClaudeTitle"), &dialog);
	auto *claude_form = new QFormLayout(claude_box);
	layout->addWidget(claude_box);

	ClaudeOptions was_options = plugin_settings_claude_options();
	auto *auto_model = new QCheckBox(text("Hindsight.AutoModel"), &dialog);
	auto_model->setChecked(was_options.auto_model);
	auto_model->setToolTip(text("Hindsight.AutoModelNote"));
	claude_form->addRow(auto_model);

	auto *model = new QComboBox(&dialog);
	for (const ClaudeModel &choice : kClaudeModels) {
		model->addItem(QString::fromUtf8(choice.label), QString::fromUtf8(choice.id));
		model->setItemData(model->count() - 1,
				   text("Hindsight.ModelPrice")
					   .arg(QString::number(choice.input_dollars_per_million, 'f', 2),
						QString::number(choice.output_dollars_per_million, 'f', 2)),
				   Qt::ToolTipRole);
	}
	model->setCurrentIndex(model->findData(QString::fromUtf8(was_options.model.c_str())));
	model->setToolTip(text("Hindsight.ClaudeModelNote"));
	claude_form->addRow(text("Hindsight.ClaudeModel"), model);
	// The dropdown is only for picking the model by hand.
	claude_form->setRowVisible(model, !was_options.auto_model);
	QObject::connect(auto_model, &QCheckBox::toggled, &dialog,
			 [claude_form, model](bool on) { claude_form->setRowVisible(model, !on); });

	auto *web_search = new QCheckBox(text("Hindsight.WebSearch"), &dialog);
	web_search->setChecked(was_options.web_search);
	web_search->setToolTip(text("Hindsight.WebSearchNote"));
	claude_form->addRow(web_search);

	// More: the update check setting and the saved clips.
	auto *more_box = new QGroupBox(text("Hindsight.MoreTitle"), &dialog);
	auto *more = new QVBoxLayout(more_box);
	layout->addWidget(more_box);

	// The update check reads this once when OBS starts, so a change here
	// takes effect at the next start.
	bool was_checking = plugin_settings_check_for_updates();
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

	add_footer(layout, &dialog);

	auto *buttons = new QDialogButtonBox(
		QDialogButtonBox::Save | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	// Saves what was typed and returns whether it worked. Every widget lives
	// until exec() returns, so capturing by reference is safe.
	std::string was_user_id = plugin_settings_discord_user_id();
	bool was_own_bot = plugin_settings_use_own_bot();
	auto save = [&]() -> bool {
		Secrets typed;
		typed.claude_api_key = claude.edit->text().toStdString();
		const bool checking = check_updates->isChecked();
		ClaudeOptions options;
		options.auto_model = auto_model->isChecked();
		options.model = model->currentData().toString().toStdString();
		options.web_search = web_search->isChecked();

#ifdef HINDSIGHT_ACCESS_CODES
		typed.hindsight_code = code.edit->text().toStdString();
		const std::string typed_code = clean_key(typed.hindsight_code);
		if (!typed_code.empty() && !is_hindsight_code(typed_code)) {
			error->setText(text("Hindsight.BadCode"));
			error->show();
			return false;
		}
#endif
		typed.discord_bot_token = discord.edit->text().toStdString();
		const std::string typed_user_id = clean_key(user_id->text().toStdString());
		if (!typed_user_id.empty() && !is_discord_user_id(typed_user_id)) {
			error->setText(text("Hindsight.BadUserId"));
			error->show();
			return false;
		}
#ifdef HINDSIGHT_ACCESS_CODES
		const bool use_own_bot = *own_bot;
#else
		const bool use_own_bot = false; // ignored: these builds always use the bot
#endif

		// Blank boxes keep the saved key. On failure the window stays open
		// so nothing typed is lost.
		const bool setting_saved = checking == was_checking || plugin_settings_set_check_for_updates(checking);
		const bool options_saved =
			(options.auto_model == was_options.auto_model && options.model == was_options.model &&
			 options.web_search == was_options.web_search) ||
			plugin_settings_set_claude_options(options);
		const bool discord_saved = (typed_user_id == was_user_id && use_own_bot == was_own_bot) ||
					   plugin_settings_set_discord(typed_user_id, use_own_bot);
		const bool keys_saved = secret_store_save_typed(typed);
		if (!setting_saved || !options_saved || !discord_saved || !keys_saved) {
			error->setText(text("Hindsight.SaveFailed"));
			error->show();
			return false;
		}
		was_checking = checking;
		was_options = options;
		was_user_id = typed_user_id;
		was_own_bot = use_own_bot;
		user_id->setText(QString::fromStdString(typed_user_id));
		return true;
	};
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
		if (save())
			dialog.accept();
	});
	// Apply saves and keeps the window open, showing the keys as saved.
	QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&]() {
		if (!save())
			return;
		error->hide();
		const Secrets now = secret_store_get();
		claude.edit->clear();
		show_saved_state(claude, !now.claude_api_key.empty());
#ifdef HINDSIGHT_ACCESS_CODES
		code.edit->clear();
		show_saved_state(code, !now.hindsight_code.empty());
#endif
		discord.edit->clear();
		show_saved_state(discord, !now.discord_bot_token.empty());
#ifdef HINDSIGHT_ACCESS_CODES
		show_delivery();
#endif
	});

	dialog.exec();
}

} // namespace

void register_settings_dialog()
{
	obs_frontend_add_tools_menu_item(obs_module_text("Hindsight.SettingsMenu"), open_settings_dialog, nullptr);
}
