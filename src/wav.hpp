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

#include <cstdint>
#include <vector>

// Encodes mono samples in [-1, 1] as a 16-bit PCM WAV file, which any audio
// player opens. Samples outside that range are clipped, and NaN or infinite
// samples become silence. Kept free of OBS so
// the tests can run it on its own.
std::vector<uint8_t> encode_wav(const std::vector<float> &samples, uint32_t sample_rate);
