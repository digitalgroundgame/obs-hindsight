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
#include "sha256.hpp"

#include <cstdio>
#include <string>

namespace {

#ifdef _WIN32
// Test vectors from FIPS 180-2.
void hashes_known_inputs()
{
	Sha256 empty;
	CHECK(empty.finish() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

	Sha256 abc;
	abc.add("abc", 3);
	CHECK(abc.finish() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

void hashes_input_fed_in_pieces()
{
	const std::string text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	Sha256 hash;
	hash.add(text.data(), 10);
	hash.add(text.data() + 10, 0);
	hash.add(text.data() + 10, text.size() - 10);
	CHECK(hash.finish() == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

void finishes_only_once()
{
	Sha256 hash;
	hash.add("abc", 3);
	CHECK(!hash.finish().empty());
	CHECK(hash.finish().empty());
}
#endif

} // namespace

int main()
{
#ifdef _WIN32
	hashes_known_inputs();
	hashes_input_fed_in_pieces();
	finishes_only_once();
#else
	std::printf("Sha256 is Windows only; nothing to check here\n");
#endif
	return check::finish();
}
