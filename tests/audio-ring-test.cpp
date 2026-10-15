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
#include "check.hpp"

#include <vector>

namespace {

void push_mono(AudioRing &ring, const std::vector<float> &samples)
{
	const float *planes[] = {samples.data()};
	ring.push_planar(planes, 1, samples.size());
}

void starts_empty()
{
	AudioRing ring;
	CHECK(ring.capacity() == 0);
	CHECK(ring.snapshot().empty());

	// Audio that arrives before the ring has a size is ignored, not a crash.
	push_mono(ring, {1.0f, 2.0f});
	CHECK(ring.snapshot().empty());
}

void keeps_everything_until_full()
{
	AudioRing ring;
	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f, 3.0f});
	CHECK(ring.size() == 3);
	CHECK(ring.snapshot() == (std::vector<float>{1.0f, 2.0f, 3.0f}));
}

void drops_oldest_once_full()
{
	AudioRing ring;
	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f, 3.0f});
	push_mono(ring, {4.0f, 5.0f, 6.0f});
	CHECK(ring.size() == 4);
	CHECK(ring.snapshot() == (std::vector<float>{3.0f, 4.0f, 5.0f, 6.0f}));

	// Wrapping many times over still returns the newest samples in order.
	for (int i = 7; i <= 20; i++)
		push_mono(ring, {static_cast<float>(i)});
	CHECK(ring.snapshot() == (std::vector<float>{17.0f, 18.0f, 19.0f, 20.0f}));
}

void mixes_channels_to_mono()
{
	AudioRing ring;
	ring.reset(8);
	const std::vector<float> left = {1.0f, 0.5f};
	const std::vector<float> right = {0.0f, -0.5f};
	const float *planes[] = {left.data(), right.data()};
	ring.push_planar(planes, 2, 2);
	CHECK(ring.snapshot() == (std::vector<float>{0.5f, 0.0f}));
}

void treats_missing_channel_as_silence()
{
	AudioRing ring;
	ring.reset(8);
	const std::vector<float> left = {1.0f, 1.0f};
	const float *planes[] = {left.data(), nullptr};
	ring.push_planar(planes, 2, 2);
	CHECK(ring.snapshot() == (std::vector<float>{0.5f, 0.5f}));
}

void reset_clears_buffered_audio()
{
	AudioRing ring;
	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f});
	ring.reset(6);
	CHECK(ring.capacity() == 6);
	CHECK(ring.snapshot().empty());
}

void copy_to_matches_snapshot_after_wrapping()
{
	AudioRing ring;
	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f});

	std::vector<float> out;
	ring.copy_to(out);
	CHECK(out == ring.snapshot());
	CHECK(out == (std::vector<float>{3.0f, 4.0f, 5.0f, 6.0f}));
}

void copy_to_reuses_reserved_space()
{
	AudioRing ring;
	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f});

	// The filter reserves before taking its lock; the copy must then not
	// allocate, or the lock is held through a large allocation again.
	std::vector<float> out = {9.0f, 9.0f};
	out.reserve(ring.capacity());
	const float *const reserved = out.data();
	ring.copy_to(out);
	CHECK(out.data() == reserved);
	CHECK(out == (std::vector<float>{2.0f, 3.0f, 4.0f, 5.0f}));
}

void copy_to_empties_out_for_an_unsized_ring()
{
	AudioRing ring;
	std::vector<float> out = {1.0f};
	ring.copy_to(out);
	CHECK(out.empty());
}

void clear_keeps_capacity()
{
	AudioRing ring;
	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f});
	ring.clear();
	CHECK(ring.capacity() == 4);
	CHECK(ring.size() == 0);
	CHECK(ring.snapshot().empty());

	push_mono(ring, {6.0f, 7.0f});
	CHECK(ring.snapshot() == (std::vector<float>{6.0f, 7.0f}));
}

} // namespace

int main()
{
	starts_empty();
	keeps_everything_until_full();
	drops_oldest_once_full();
	mixes_channels_to_mono();
	treats_missing_channel_as_silence();
	reset_clears_buffered_audio();
	copy_to_matches_snapshot_after_wrapping();
	copy_to_reuses_reserved_space();
	copy_to_empties_out_for_an_unsized_ring();
	clear_keeps_capacity();
	return check::finish();
}
