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

#pragma once

#include <string>

// Standard base64 (RFC 4648) with '=' padding. Kept free of OBS so the tests
// can run it on its own.
std::string base64_encode(const std::string &bytes);

// Returns false, leaving out empty, if text is not valid padded base64.
bool base64_decode(const std::string &text, std::string &out);
