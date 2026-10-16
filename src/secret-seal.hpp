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

// Whether this platform can seal the keys at all (only Windows, for now).
bool can_seal_secrets();

// Turns the secrets text into the contents of the secrets file and back.
//
// On Windows the text is encrypted with DPAPI for the signed-in Windows user,
// so a copy of the file (in a backup, or a zipped OBS folder) is useless on
// another account or PC. The file is one line, "dpapi:<base64>". Other
// platforms are stubs that always fail until #15 adds their keychains, so
// keys are never stored unencrypted. Free of OBS so the tests can run it on
// its own.
//
// Returns "" if the text could not be sealed.
std::string seal_secrets_text(const std::string &text);

// Returns false, leaving text empty, if the contents are not a file
// seal_secrets_text wrote on this platform, or cannot be decrypted (for
// example, a file copied from another Windows account).
bool open_secrets_text(const std::string &contents, std::string &text);
