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

#include "check.hpp"
#include "wav.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

uint32_t read_le(const std::vector<uint8_t> &bytes, size_t offset, int count)
{
	uint32_t value = 0;
	for (int i = 0; i < count; i++)
		value |= static_cast<uint32_t>(bytes[offset + i]) << (8 * i);
	return value;
}

int16_t sample_at(const std::vector<uint8_t> &bytes, size_t index)
{
	return static_cast<int16_t>(read_le(bytes, 44 + index * 2, 2));
}

bool has_tag(const std::vector<uint8_t> &bytes, size_t offset, const char *tag)
{
	return std::memcmp(bytes.data() + offset, tag, 4) == 0;
}

void writes_standard_header()
{
	const std::vector<uint8_t> wav = encode_wav({0.0f, 0.0f, 0.0f}, 48000);
	CHECK(wav.size() == 44 + 3 * 2);
	CHECK(has_tag(wav, 0, "RIFF"));
	CHECK(read_le(wav, 4, 4) == wav.size() - 8);
	CHECK(has_tag(wav, 8, "WAVE"));
	CHECK(has_tag(wav, 12, "fmt "));
	CHECK(read_le(wav, 16, 4) == 16);    // fmt chunk size
	CHECK(read_le(wav, 20, 2) == 1);     // PCM
	CHECK(read_le(wav, 22, 2) == 1);     // mono
	CHECK(read_le(wav, 24, 4) == 48000); // sample rate
	CHECK(read_le(wav, 28, 4) == 96000); // byte rate
	CHECK(read_le(wav, 32, 2) == 2);     // block align
	CHECK(read_le(wav, 34, 2) == 16);    // bits per sample
	CHECK(has_tag(wav, 36, "data"));
	CHECK(read_le(wav, 40, 4) == 3 * 2); // data size
}

void empty_audio_is_header_only()
{
	const std::vector<uint8_t> wav = encode_wav({}, 44100);
	CHECK(wav.size() == 44);
	CHECK(read_le(wav, 40, 4) == 0);
}

void converts_samples_to_16_bit()
{
	const std::vector<uint8_t> wav = encode_wav({0.0f, 1.0f, -1.0f, 0.5f}, 48000);
	CHECK(sample_at(wav, 0) == 0);
	CHECK(sample_at(wav, 1) == 32767);
	CHECK(sample_at(wav, 2) == -32767);
	CHECK(sample_at(wav, 3) == 16383);
}

void clips_loud_samples()
{
	const std::vector<uint8_t> wav = encode_wav({2.0f, -3.0f}, 48000);
	CHECK(sample_at(wav, 0) == 32767);
	CHECK(sample_at(wav, 1) == -32767);
}

} // namespace

int main()
{
	writes_standard_header();
	empty_audio_is_header_only();
	converts_samples_to_16_bit();
	clips_loud_samples();
	return check::finish();
}
