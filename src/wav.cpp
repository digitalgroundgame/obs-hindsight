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

#include "wav.hpp"

#include <algorithm>
#include <cmath>

namespace {

void put_le(std::vector<uint8_t> &out, uint32_t value, int bytes)
{
	for (int i = 0; i < bytes; i++)
		out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
}

void put_tag(std::vector<uint8_t> &out, const char *tag)
{
	out.insert(out.end(), tag, tag + 4);
}

} // namespace

std::vector<uint8_t> encode_wav(const std::vector<float> &samples, uint32_t sample_rate)
{
	if (!wav_fits(samples.size()))
		return {};

	const uint32_t data_bytes = static_cast<uint32_t>(samples.size() * 2);

	std::vector<uint8_t> out;
	out.reserve(44 + static_cast<std::size_t>(data_bytes));
	put_tag(out, "RIFF");
	put_le(out, 36 + data_bytes, 4);
	put_tag(out, "WAVE");
	put_tag(out, "fmt ");
	put_le(out, 16, 4);              // fmt chunk size
	put_le(out, 1, 2);               // PCM
	put_le(out, 1, 2);               // mono
	put_le(out, sample_rate, 4);     // sample rate
	put_le(out, sample_rate * 2, 4); // byte rate
	put_le(out, 2, 2);               // block align
	put_le(out, 16, 2);              // bits per sample
	put_tag(out, "data");
	put_le(out, data_bytes, 4);

	for (float sample : samples) {
		// NaN or infinity would make the int16_t cast undefined; write silence.
		const float clamped = std::isfinite(sample) ? std::clamp(sample, -1.0f, 1.0f) : 0.0f;
		put_le(out, static_cast<uint16_t>(static_cast<int16_t>(clamped * 32767.0f)), 2);
	}

	return out;
}
