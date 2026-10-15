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

#include "check.hpp"
#include "secrets.hpp"

#include <string>

namespace {

void round_trips()
{
	Secrets secrets;
	secrets.claude_api_key = "sk-ant-abc123";
	secrets.discord_bot_token = "MTA.xyz-789";

	const Secrets decoded = decode_secrets(encode_secrets(secrets));
	CHECK(decoded.claude_api_key == "sk-ant-abc123");
	CHECK(decoded.discord_bot_token == "MTA.xyz-789");
}

void round_trips_empty_keys()
{
	const Secrets decoded = decode_secrets(encode_secrets(Secrets{}));
	CHECK(decoded.claude_api_key.empty());
	CHECK(decoded.discord_bot_token.empty());
}

void cleans_pasted_whitespace()
{
	CHECK(clean_key("  sk-ant-abc\r\n") == "sk-ant-abc");
	CHECK(clean_key("sk ant\tabc") == "skantabc");
	CHECK(clean_key(" \n ").empty());
}

void newline_in_a_key_cannot_add_a_line()
{
	// A key with an embedded newline must not smuggle in another entry.
	Secrets secrets;
	secrets.claude_api_key = "abc\ndiscord_bot_token=stolen";

	const Secrets decoded = decode_secrets(encode_secrets(secrets));
	CHECK(decoded.claude_api_key == "abcdiscord_bot_token=stolen");
	CHECK(decoded.discord_bot_token.empty());
}

void decode_skips_junk()
{
	const Secrets decoded = decode_secrets("\n"
					       "no equals sign here\n"
					       "unknown_key=ignored\n"
					       "claude_api_key = sk-ant-1 \r\n"
					       "discord_bot_token=tok=with=equals");
	CHECK(decoded.claude_api_key == "sk-ant-1");
	CHECK(decoded.discord_bot_token == "tok=with=equals");
}

void decode_empty_text()
{
	const Secrets decoded = decode_secrets("");
	CHECK(decoded.claude_api_key.empty());
	CHECK(decoded.discord_bot_token.empty());
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

} // namespace

int main()
{
	round_trips();
	round_trips_empty_keys();
	cleans_pasted_whitespace();
	newline_in_a_key_cannot_add_a_line();
	decode_skips_junk();
	decode_empty_text();
	merge_takes_typed_keys();
	merge_keeps_saved_keys_for_blank_fields();
	merge_reports_no_change_for_the_same_key();
	return check::finish();
}
