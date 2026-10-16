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
#include "version-check.hpp"

namespace {

void parses_plain_versions()
{
	const auto version = parse_release_version("1.20.3");
	CHECK(version.has_value());
	if (!version)
		return;
	CHECK(version->major == 1);
	CHECK(version->minor == 20);
	CHECK(version->patch == 3);
	CHECK(version->stage == ReleaseVersion::Stage::release);
}

void parses_prereleases_and_a_leading_v()
{
	const auto beta = parse_release_version("v0.2.0-beta1");
	CHECK(beta.has_value());
	if (beta) {
		CHECK(beta->stage == ReleaseVersion::Stage::beta);
		CHECK(beta->stage_number == 1);
	}

	const auto rc = parse_release_version("0.2.0-rc");
	CHECK(rc.has_value());
	if (rc) {
		CHECK(rc->stage == ReleaseVersion::Stage::rc);
		CHECK(rc->stage_number == 0);
	}
}

void rejects_anything_else()
{
	CHECK(!parse_release_version(""));
	CHECK(!parse_release_version("1.2"));
	CHECK(!parse_release_version("1.2.3.4"));
	CHECK(!parse_release_version("1.2.x"));
	CHECK(!parse_release_version("1..3"));
	CHECK(!parse_release_version("-1.2.3"));
	CHECK(!parse_release_version("1.2.3-alpha1"));
	CHECK(!parse_release_version("1.2.3-beta1x"));
	CHECK(!parse_release_version("1.2.3 "));
	CHECK(!parse_release_version("vv1.2.3"));
	CHECK(!parse_release_version("1234567.0.0"));
}

void orders_versions()
{
	CHECK(update_available("0.1.1", "0.1.0"));
	CHECK(update_available("0.2.0", "0.1.9"));
	CHECK(update_available("1.0.0", "0.99.99"));
	CHECK(update_available("0.10.0", "0.9.0"));
	CHECK(!update_available("0.1.0", "0.1.0"));
	CHECK(!update_available("0.1.0", "0.2.0"));
}

void orders_prereleases_before_the_release()
{
	CHECK(update_available("0.2.0-beta2", "0.2.0-beta1"));
	CHECK(update_available("0.2.0-rc1", "0.2.0-beta9"));
	CHECK(update_available("0.2.0", "0.2.0-rc1"));
	CHECK(!update_available("0.2.0-rc1", "0.2.0"));
	CHECK(update_available("0.2.0-beta1", "0.1.0"));
}

void ignores_versions_it_cannot_read()
{
	CHECK(!update_available("latest", "0.1.0"));
	CHECK(!update_available("9.9.9", "dev"));
	CHECK(!update_available("", ""));
}

} // namespace

int main()
{
	parses_plain_versions();
	parses_prereleases_and_a_leading_v();
	rejects_anything_else();
	orders_versions();
	orders_prereleases_before_the_release();
	ignores_versions_it_cannot_read();
	return check::finish();
}
