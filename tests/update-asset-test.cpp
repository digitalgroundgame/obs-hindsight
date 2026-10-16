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
#include "update-asset.hpp"

namespace {

void names_the_windows_installer()
{
	CHECK(windows_installer_name("0.2.1") == "obs-hindsight-0.2.1-windows-x64-installer.exe");
	CHECK(windows_installer_name("1.0.0-rc1") == "obs-hindsight-1.0.0-rc1-windows-x64-installer.exe");
}

void names_nothing_for_tags_it_cannot_read()
{
	CHECK(windows_installer_name("").empty());
	CHECK(windows_installer_name("latest").empty());
	CHECK(windows_installer_name("0.2.1/../../evil").empty());
	CHECK(windows_installer_name("0.2.1\\evil").empty());
}

void reads_sha256_digests()
{
	const std::string hex = "704b9e3f0a3b32c360ce5cac66bf4a3e7669edd15a34d8ec561789f66acca262";
	CHECK(sha256_from_digest("sha256:" + hex) == hex);
	CHECK(sha256_from_digest("sha256:704B9E3F0A3B32C360CE5CAC66BF4A3E7669EDD15A34D8EC561789F66ACCA262") == hex);
}

void rejects_other_digests()
{
	const std::string hex = "704b9e3f0a3b32c360ce5cac66bf4a3e7669edd15a34d8ec561789f66acca262";
	CHECK(!sha256_from_digest(""));
	CHECK(!sha256_from_digest(hex));
	CHECK(!sha256_from_digest("sha1:" + hex));
	CHECK(!sha256_from_digest("sha256:" + hex.substr(1)));
	CHECK(!sha256_from_digest("sha256:" + hex + "0"));
	CHECK(!sha256_from_digest("sha256:" + hex.substr(1) + "g"));
}

} // namespace

int main()
{
	names_the_windows_installer();
	names_nothing_for_tags_it_cannot_read();
	reads_sha256_digests();
	rejects_other_digests();
	return check::finish();
}
