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

#include <cstddef>
#include <utility>
#include <vector>

// Counted by bmem-shim.cpp.
extern std::size_t bmem_shim_allocations;

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

void copy_to_shrinks_a_longer_out()
{
	AudioRing ring;
	ring.reset(2);
	push_mono(ring, {1.0f, 2.0f, 3.0f});

	std::vector<float> out = {9.0f, 9.0f, 9.0f, 9.0f, 9.0f};
	ring.copy_to(out);
	CHECK(out == (std::vector<float>{2.0f, 3.0f}));
}

void copy_to_takes_only_the_newest()
{
	AudioRing ring;
	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f});

	std::vector<float> out;
	ring.copy_to(out, 2);
	CHECK(out == (std::vector<float>{4.0f, 5.0f}));
	ring.copy_to(out, 0);
	CHECK(out.empty());
	// Asking for more than is buffered copies everything.
	ring.copy_to(out, 10);
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

void clear_and_reset_zero_are_safe_when_empty()
{
	AudioRing ring;
	ring.clear();
	CHECK(ring.size() == 0);

	ring.reset(4);
	push_mono(ring, {1.0f, 2.0f});
	ring.reset(0);
	CHECK(ring.capacity() == 0);
	CHECK(ring.size() == 0);
	push_mono(ring, {3.0f});
	CHECK(ring.snapshot().empty());
}

void move_takes_the_audio()
{
	AudioRing from;
	from.reset(4);
	push_mono(from, {1.0f, 2.0f, 3.0f});

	AudioRing to(std::move(from));
	CHECK(to.capacity() == 4);
	CHECK(to.snapshot() == (std::vector<float>{1.0f, 2.0f, 3.0f}));
	// Checks the moved-from ring on purpose.
	// NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	CHECK(from.capacity() == 0);
	CHECK(from.size() == 0);
	// The moved-from ring still works once given a size again.
	from.reset(2);
	push_mono(from, {4.0f, 5.0f, 6.0f});
	CHECK(from.snapshot() == (std::vector<float>{5.0f, 6.0f}));
	// NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
}

void swap_exchanges_the_audio()
{
	// The filter resizes by building a new ring and swapping it in.
	AudioRing live;
	live.reset(4);
	push_mono(live, {1.0f, 2.0f});
	AudioRing resized;
	resized.reset(8);

	std::swap(live, resized);
	CHECK(live.capacity() == 8);
	CHECK(live.size() == 0);
	CHECK(resized.capacity() == 4);
	CHECK(resized.snapshot() == (std::vector<float>{1.0f, 2.0f}));

	push_mono(live, {3.0f});
	CHECK(live.snapshot() == (std::vector<float>{3.0f}));
}

void take_newest_from_keeps_all_when_growing()
{
	AudioRing live;
	live.reset(4);
	push_mono(live, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f});

	AudioRing resized;
	resized.reset(8);
	const std::size_t allocations = bmem_shim_allocations;
	resized.take_newest_from(live);
	CHECK(bmem_shim_allocations == allocations);
	CHECK(resized.capacity() == 8);
	CHECK(resized.snapshot() == (std::vector<float>{3.0f, 4.0f, 5.0f, 6.0f}));

	// The carried audio is the start of the new window, not the whole of it.
	push_mono(resized, {7.0f, 8.0f, 9.0f, 10.0f, 11.0f});
	CHECK(bmem_shim_allocations == allocations);
	CHECK(resized.snapshot() == (std::vector<float>{4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f}));
	CHECK(live.snapshot() == (std::vector<float>{3.0f, 4.0f, 5.0f, 6.0f}));
}

void take_newest_from_keeps_newest_when_shrinking()
{
	AudioRing live;
	live.reset(6);
	push_mono(live, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f});

	AudioRing resized;
	resized.reset(3);
	const std::size_t allocations = bmem_shim_allocations;
	resized.take_newest_from(live);
	CHECK(bmem_shim_allocations == allocations);
	CHECK(resized.capacity() == 3);
	CHECK(resized.snapshot() == (std::vector<float>{6.0f, 7.0f, 8.0f}));

	push_mono(resized, {9.0f});
	CHECK(bmem_shim_allocations == allocations);
	CHECK(resized.snapshot() == (std::vector<float>{7.0f, 8.0f, 9.0f}));
}

void take_newest_from_replaces_what_was_there()
{
	AudioRing live;
	live.reset(4);
	push_mono(live, {1.0f, 2.0f});

	// A ring that has wrapped, so its own audio does not start at the front.
	AudioRing resized;
	resized.reset(3);
	push_mono(resized, {7.0f, 8.0f, 9.0f, 10.0f});
	resized.take_newest_from(live);
	CHECK(resized.snapshot() == (std::vector<float>{1.0f, 2.0f}));
	// Taking from itself keeps its audio.
	resized.take_newest_from(resized);
	CHECK(resized.snapshot() == (std::vector<float>{1.0f, 2.0f}));

	// Nothing to take leaves it empty.
	AudioRing empty;
	empty.reset(4);
	resized.take_newest_from(empty);
	CHECK(resized.size() == 0);

	// An unsized ring has no room to take anything into.
	AudioRing unsized;
	unsized.take_newest_from(live);
	CHECK(unsized.snapshot().empty());
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
	copy_to_shrinks_a_longer_out();
	copy_to_takes_only_the_newest();
	copy_to_empties_out_for_an_unsized_ring();
	clear_keeps_capacity();
	clear_and_reset_zero_are_safe_when_empty();
	move_takes_the_audio();
	swap_exchanges_the_audio();
	take_newest_from_keeps_all_when_growing();
	take_newest_from_keeps_newest_when_shrinking();
	take_newest_from_replaces_what_was_there();
	return check::finish();
}
