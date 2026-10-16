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

// libobs's allocator for the unit tests. AudioRing keeps its samples in
// libobs's header-only deque, which allocates through bmalloc and friends;
// the plugin gets them from libobs, the tests from here.

#include <util/bmem.h>

#include <cstddef>
#include <cstdlib>

// How many times the deque asked for memory, so a test can check that
// something did not allocate. Declared where it is used.
std::size_t bmem_shim_allocations = 0;

extern "C" {

void *bmalloc(size_t size)
{
	bmem_shim_allocations++;
	return std::malloc(size ? size : 1);
}

void *brealloc(void *ptr, size_t size)
{
	bmem_shim_allocations++;
	return std::realloc(ptr, size ? size : 1);
}

void bfree(void *ptr)
{
	std::free(ptr);
}

} // extern "C"
