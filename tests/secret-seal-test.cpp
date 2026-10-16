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
#include "secret-seal.hpp"

#include <string>

namespace {

void round_trips_the_secrets_text()
{
	const std::string json = "{\"claude_api_key\": \"sk-ant-abc123\"}";
	const std::string sealed = seal_secrets_text(json);
	CHECK(!sealed.empty());

	std::string text;
	CHECK(open_secrets_text(sealed, text));
	CHECK(text == json);
}

void rejects_other_files()
{
	std::string text = "left over";
	CHECK(!open_secrets_text("{\"claude_api_key\": \"sk-ant-abc123\"}", text));
	CHECK(text.empty());
	CHECK(!open_secrets_text("", text));
	CHECK(!open_secrets_text("other:Zm9v\n", text));
}

} // namespace

int main()
{
	round_trips_the_secrets_text();
	rejects_other_files();
	return check::finish();
}
