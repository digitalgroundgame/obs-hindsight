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

#pragma once

// A tiny test helper so the tests need nothing beyond the compiler. CHECK
// records a failure and carries on; finish() turns failures into a non-zero
// exit code, which is what CTest looks at.

#include <cstdio>

namespace check {

inline int &failures()
{
	static int count = 0;
	return count;
}

inline int finish()
{
	if (failures() == 0) {
		std::printf("all checks passed\n");
		return 0;
	}
	std::printf("%d check(s) failed\n", failures());
	return 1;
}

} // namespace check

#define CHECK(condition)                                                                   \
	do {                                                                               \
		if (!(condition)) {                                                        \
			std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
			check::failures()++;                                               \
		}                                                                          \
	} while (0)
