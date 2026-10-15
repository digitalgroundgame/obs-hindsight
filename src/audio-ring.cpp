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

#include "audio-ring.hpp"

#include <algorithm>

void AudioRing::reset(std::size_t capacity)
{
	samples.assign(capacity, 0.0f);
	write_pos = 0;
	filled = 0;
}

void AudioRing::push_planar(const float *const *planes, std::size_t channels, std::size_t frames)
{
	const std::size_t size = samples.size();
	if (size == 0 || channels == 0)
		return;

	for (std::size_t frame = 0; frame < frames; frame++) {
		float sum = 0.0f;
		for (std::size_t ch = 0; ch < channels; ch++)
			sum += planes[ch] ? planes[ch][frame] : 0.0f;

		samples[write_pos] = sum / static_cast<float>(channels);
		write_pos = (write_pos + 1) % size;
		filled = std::min(filled + 1, size);
	}
}

std::vector<float> AudioRing::snapshot() const
{
	std::vector<float> out;
	out.reserve(filled);
	const std::size_t size = samples.size();
	if (size == 0)
		return out;

	// The buffered audio is at most two runs: from start to the end of the
	// vector, then wrapping around to the beginning.
	const std::size_t start = (write_pos + size - filled) % size;
	const std::size_t first = std::min(filled, size - start);
	const auto begin = samples.begin();
	out.insert(out.end(), begin + static_cast<std::ptrdiff_t>(start),
		   begin + static_cast<std::ptrdiff_t>(start + first));
	out.insert(out.end(), begin, begin + static_cast<std::ptrdiff_t>(filled - first));
	return out;
}
