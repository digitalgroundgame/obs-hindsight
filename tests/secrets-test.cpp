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
	CHECK(stored.hindsight_code.empty());
}

void merge_keeps_saved_keys_for_blank_fields()
{
	Secrets stored;
	stored.claude_api_key = "sk-saved";
	stored.hindsight_code = "hs-saved";
	Secrets typed;
	typed.hindsight_code = "   ";

	CHECK(!merge_typed_keys(stored, typed));
	CHECK(stored.claude_api_key == "sk-saved");
	CHECK(stored.hindsight_code == "hs-saved");
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
	old.hindsight_code = " hs-old\n";

	CHECK(fill_empty_keys(stored, old));
	CHECK(stored.claude_api_key == "sk-new");
	CHECK(stored.hindsight_code == "hs-old");

	// Nothing left to fill.
	CHECK(!fill_empty_keys(stored, old));
	CHECK(!fill_empty_keys(stored, Secrets{}));
}

void forgets_one_key()
{
	Secrets secrets;
	secrets.claude_api_key = "sk-keep";
	secrets.hindsight_code = "hs-drop";

	CHECK(forget_key(secrets, SecretKey::HindsightCode));
	CHECK(secrets.claude_api_key == "sk-keep");
	CHECK(secrets.hindsight_code.empty());

	// Forgetting an empty key reports that nothing changed.
	CHECK(!forget_key(secrets, SecretKey::HindsightCode));
	CHECK(forget_key(secrets, SecretKey::ClaudeApiKey));
	CHECK(secrets.claude_api_key.empty());
}

void recognises_hindsight_codes()
{
	const std::string random(43, 'A');
	CHECK(is_hindsight_code("hs_" + random));
	CHECK(is_hindsight_code("hs_abcdefghijklmnopqrstuvwxyzABCDEFGHIJK-_0123"));

	CHECK(!is_hindsight_code(""));
	CHECK(!is_hindsight_code("hs_"));
	CHECK(!is_hindsight_code("hs_" + random.substr(1))); // pasted one short
	CHECK(!is_hindsight_code("hs_" + random + "A"));
	CHECK(!is_hindsight_code("HS_" + random));
	CHECK(!is_hindsight_code("sk_" + random)); // the wrong key in the box
	CHECK(!is_hindsight_code("hs_" + random.substr(1) + "="));
	CHECK(!is_hindsight_code("hs_" + random.substr(1) + "+"));
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
	recognises_hindsight_codes();
	return check::finish();
}
