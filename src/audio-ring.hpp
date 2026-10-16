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

// The last few seconds of a source, mixed down to mono. Once full, each new
// sample replaces the oldest. Not thread-safe: the filter guards it with its
// own lock. Kept free of OBS so the tests can run it on its own.
class AudioRing {
public:
	// Sets how many samples to keep and drops whatever was buffered.
	void reset(std::size_t capacity);

	// Drops whatever was buffered, keeping the capacity. Never allocates.
	void clear();

	std::size_t capacity() const { return samples.size(); }
	std::size_t size() const { return filled; }

	// Mixes `frames` frames of planar audio (one array per channel, as OBS
	// hands it over) down to mono and appends them. A null plane counts as
	// silence.
	void push_planar(const float *const *planes, std::size_t channels, std::size_t frames);

	// Returns the buffered audio, oldest sample first.
	std::vector<float> snapshot() const;

	// Replaces the contents of out with the newest `newest` buffered samples
	// (all of them by default), oldest first. Does not allocate when
	// out.capacity() >= size(), so the filter can reserve the space before
	// taking its lock and hold the lock only for the copy.
	void copy_to(std::vector<float> &out, std::size_t newest = SIZE_MAX) const;

private:
	std::vector<float> samples;
	std::size_t write_pos = 0;
	std::size_t filled = 0;
};
