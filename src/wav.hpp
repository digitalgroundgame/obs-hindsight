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

#include <cstddef>
#include <cstdint>
#include <vector>

// The most samples a WAV file can hold: its size fields are 32-bit. That is
// over 12 hours at 48 kHz, far past Hindsight's 30 s window.
constexpr std::size_t kMaxWavSamples = (UINT32_MAX - 36) / 2;

// Encodes mono samples in [-1, 1] as a 16-bit PCM WAV file, which any audio
// player opens. Samples outside that range are clipped, and NaN or infinite
// samples become silence. Returns an empty vector, writing nothing, if there
// are more than kMaxWavSamples samples. Kept free of OBS so the tests can run
// it on its own.
std::vector<uint8_t> encode_wav(const std::vector<float> &samples, uint32_t sample_rate);
