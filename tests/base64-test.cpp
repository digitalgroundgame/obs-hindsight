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

#include "base64.hpp"
#include "check.hpp"

#include <string>

namespace {

void encodes_rfc4648_vectors()
{
	CHECK(base64_encode("") == "");
	CHECK(base64_encode("f") == "Zg==");
	CHECK(base64_encode("fo") == "Zm8=");
	CHECK(base64_encode("foo") == "Zm9v");
	CHECK(base64_encode("foob") == "Zm9vYg==");
	CHECK(base64_encode("fooba") == "Zm9vYmE=");
	CHECK(base64_encode("foobar") == "Zm9vYmFy");
}

void round_trips_every_byte_value()
{
	std::string bytes;
	for (int i = 0; i < 256; i++)
		bytes.push_back(static_cast<char>(i));

	std::string decoded;
	CHECK(base64_decode(base64_encode(bytes), decoded));
	CHECK(decoded == bytes);
}

void rejects_malformed_text()
{
	std::string out = "left over";
	CHECK(!base64_decode("Zm9", out));
	CHECK(out.empty());
	CHECK(!base64_decode("Zm9*", out));
	CHECK(!base64_decode("Zg==Zm9v", out));
	CHECK(!base64_decode("Z===", out));
	CHECK(!base64_decode("Zm=v", out));
	CHECK(base64_decode("", out));
	CHECK(out.empty());
}

} // namespace

int main()
{
	encodes_rfc4648_vectors();
	round_trips_every_byte_value();
	rejects_malformed_text();
	return check::finish();
}
