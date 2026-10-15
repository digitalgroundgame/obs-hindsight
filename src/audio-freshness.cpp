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

#include "audio-freshness.hpp"

#include <algorithm>

bool audio_resumed_after_gap(uint64_t last_push_ns, uint64_t now_ns)
{
	return last_push_ns != 0 && now_ns > last_push_ns && now_ns - last_push_ns > kAudioGapNs;
}

std::size_t fresh_sample_count(std::size_t buffered, std::size_t window_samples, uint32_t sample_rate,
			       uint64_t last_push_ns, uint64_t now_ns)
{
	if (last_push_ns == 0 || buffered == 0 || sample_rate == 0)
		return 0;

	const uint64_t silence_ns = now_ns > last_push_ns ? now_ns - last_push_ns : 0;

	// Whole seconds first, so hours of silence cannot overflow the multiply.
	const uint64_t silence_seconds = silence_ns / 1'000'000'000;
	if (silence_seconds >= window_samples / sample_rate + 1)
		return 0;
	const uint64_t silence_samples =
		silence_seconds * sample_rate + (silence_ns % 1'000'000'000) * sample_rate / 1'000'000'000;

	if (silence_samples >= window_samples)
		return 0;
	return std::min(buffered, window_samples - static_cast<std::size_t>(silence_samples));
}
