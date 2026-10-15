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

void sealed_file_does_not_show_the_keys()
{
	const std::string sealed = seal_secrets_text("claude_api_key=sk-ant-abc123\n");
	CHECK(sealed.find("sk-ant") == std::string::npos);
	CHECK(sealed.find("claude_api_key") == std::string::npos);
}

void rejects_other_files()
{
	std::string text = "left over";
	CHECK(!open_secrets_text("claude_api_key=sk-ant-abc123\n", text));
	CHECK(text.empty());
	CHECK(!open_secrets_text("", text));
	CHECK(!open_secrets_text("other:Zm9v\n", text));
	// The right scheme with broken base64 is not a file we wrote.
	const std::string sealed = seal_secrets_text("x");
	const std::string scheme = sealed.substr(0, sealed.find(':') + 1);
	CHECK(!open_secrets_text(scheme + "***\n", text));
}

void accepts_windows_line_endings()
{
	std::string sealed = seal_secrets_text("claude_api_key=k\n");
	sealed.insert(sealed.size() - 1, "\r");

	std::string text;
	CHECK(open_secrets_text(sealed, text));
	CHECK(text == "claude_api_key=k\n");
}

} // namespace

int main()
{
	round_trips_the_secrets_text();
	sealed_file_does_not_show_the_keys();
	rejects_other_files();
	accepts_windows_line_endings();
	return check::finish();
}
