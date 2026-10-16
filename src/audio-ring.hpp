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

#include <util/deque.h>

#include <cstddef>
#include <cstdint>
#include <vector>

// The last few seconds of a source, mixed down to mono. Once full, each new
// sample replaces the oldest. Not thread-safe: the filter guards it with its
// own lock.
//
// The samples live in libobs's deque (util/deque.h), the queue OBS itself
// buffers audio in. It is header-only apart from libobs's allocator, so the
// unit tests build it with a small stand-in allocator instead of all of OBS.
class AudioRing {
public:
	AudioRing();
	~AudioRing();
	AudioRing(AudioRing &&other) noexcept;
	AudioRing &operator=(AudioRing &&other) noexcept;
	AudioRing(const AudioRing &) = delete;
	AudioRing &operator=(const AudioRing &) = delete;

	// Sets how many samples to keep and drops whatever was buffered. This
	// allocates all the space up front, so pushing never allocates.
	void reset(std::size_t capacity);

	// Drops whatever was buffered, keeping the capacity. Never allocates.
	void clear();

	std::size_t capacity() const { return max_samples; }
	std::size_t size() const { return queue.size / sizeof(float); }

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

	// Replaces the buffered audio with the newest samples of other, as many
	// as fit, oldest first. Never allocates: reset() already reserved the
	// space. Lets the filter resize the ring without losing what it heard.
	void take_newest_from(const AudioRing &other);

private:
	// Sized in bytes, like every libobs deque; size() converts to samples.
	deque queue;
	std::size_t max_samples = 0;
};
