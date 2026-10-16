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

#include <cstdio>
#include <string>

// A file that create_new_file made, and the path it chose. The caller owns
// file and must fclose it. file is null if no file could be made.
struct NewFile {
	FILE *file = nullptr;
	std::string path;
};

// Makes a new file in folder named name + extension (the extension includes
// the dot), or name-2, name-3 and so on if that one exists, and opens it for
// writing in binary mode. Checking that a name is free and taking it are one
// step, so two writers saving at once (say, Hindsight on two mics) can never
// pick the same name and write over each other, and an existing file is never
// emptied. Gives up after a few hundred names, or at once on any error other
// than the name being taken (such as a missing folder). Paths are UTF-8 on
// every OS. Free of OBS so the tests can run it on its own.
NewFile create_new_file(const std::string &folder, const std::string &name, const std::string &extension);
