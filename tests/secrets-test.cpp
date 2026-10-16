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

#include "check.hpp"
#include "secrets.hpp"

#include <string>

namespace {

void cleans_pasted_whitespace()
{
	CHECK(clean_key("  sk-ant-abc\r\n") == "sk-ant-abc");
	CHECK(clean_key("sk ant\tabc") == "skantabc");
	CHECK(clean_key(" \n ").empty());
}

void merge_takes_typed_keys()
{
	Secrets stored;
	Secrets typed;
	typed.claude_api_key = " sk-new ";

	CHECK(merge_typed_keys(stored, typed));
	CHECK(stored.claude_api_key == "sk-new");
	CHECK(stored.discord_bot_token.empty());
}

void merge_keeps_saved_keys_for_blank_fields()
{
	Secrets stored;
	stored.claude_api_key = "sk-saved";
	stored.discord_bot_token = "tok-saved";
	Secrets typed;
	typed.discord_bot_token = "   ";

	CHECK(!merge_typed_keys(stored, typed));
	CHECK(stored.claude_api_key == "sk-saved");
	CHECK(stored.discord_bot_token == "tok-saved");
}

void merge_reports_no_change_for_the_same_key()
{
	Secrets stored;
	stored.claude_api_key = "sk-same";
	Secrets typed;
	typed.claude_api_key = "sk-same\n";

	CHECK(!merge_typed_keys(stored, typed));
}

void old_keys_fill_only_empty_slots()
{
	Secrets stored;
	stored.claude_api_key = "sk-new";
	Secrets old;
	old.claude_api_key = "sk-old";
	old.discord_bot_token = " tok-old\n";

	CHECK(fill_empty_keys(stored, old));
	CHECK(stored.claude_api_key == "sk-new");
	CHECK(stored.discord_bot_token == "tok-old");

	// Nothing left to fill.
	CHECK(!fill_empty_keys(stored, old));
	CHECK(!fill_empty_keys(stored, Secrets{}));
}

void forgets_one_key()
{
	Secrets secrets;
	secrets.claude_api_key = "sk-keep";
	secrets.discord_bot_token = "tok-drop";

	CHECK(forget_key(secrets, SecretKey::DiscordBotToken));
	CHECK(secrets.claude_api_key == "sk-keep");
	CHECK(secrets.discord_bot_token.empty());

	// Forgetting an empty key reports that nothing changed.
	CHECK(!forget_key(secrets, SecretKey::DiscordBotToken));
	CHECK(forget_key(secrets, SecretKey::ClaudeApiKey));
	CHECK(secrets.claude_api_key.empty());
}

void handles_the_hindsight_code()
{
	Secrets stored;
	stored.discord_bot_token = "tok-saved";
	Secrets typed;
	typed.hindsight_code = " dgc-new\n";

	CHECK(merge_typed_keys(stored, typed));
	CHECK(stored.hindsight_code == "dgc-new");
	CHECK(stored.discord_bot_token == "tok-saved");

	CHECK(forget_key(stored, SecretKey::HindsightCode));
	CHECK(stored.hindsight_code.empty());
	CHECK(stored.discord_bot_token == "tok-saved");
}

void recognises_hindsight_codes()
{
	const std::string random(43, 'A');
	CHECK(is_hindsight_code("dgc_" + random));
	CHECK(is_hindsight_code("dgc_abcdefghijklmnopqrstuvwxyzABCDEFGHIJK-_0123"));

	CHECK(!is_hindsight_code(""));
	CHECK(!is_hindsight_code("dgc_"));
	CHECK(!is_hindsight_code("dgc_" + random.substr(1))); // pasted one short
	CHECK(!is_hindsight_code("dgc_" + random + "A"));
	CHECK(!is_hindsight_code("DGC_" + random));
	CHECK(!is_hindsight_code("sk_" + random + "A")); // the wrong key in the box
	CHECK(!is_hindsight_code("dgc_" + random.substr(1) + "="));
	CHECK(!is_hindsight_code("dgc_" + random.substr(1) + "+"));
}

} // namespace

int main()
{
	cleans_pasted_whitespace();
	merge_takes_typed_keys();
	merge_keeps_saved_keys_for_blank_fields();
	merge_reports_no_change_for_the_same_key();
	old_keys_fill_only_empty_slots();
	forgets_one_key();
	handles_the_hindsight_code();
	recognises_hindsight_codes();
	return check::finish();
}
