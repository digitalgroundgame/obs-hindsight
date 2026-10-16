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

#pragma once

#include <cstddef>
#include <cstdint>

// OBS runs a filter only while its source is active. While the mic's scene is
// not showing, no audio arrives and the ring keeps what it heard last, which
// could be hours old. These decide how much of the buffer is still recent
// enough to send. Times are in nanoseconds from any steady clock (the filter
// uses os_gettime_ns). Kept free of OBS so the tests can run it on its own.

// OBS hands over audio about every 20 ms. A longer pause than this means the
// source stopped, and the old audio is dropped instead of being joined to
// the new audio with no gap. It is a few seconds rather than a fraction of
// one so a brief stall on OBS's audio thread (a heavy scene switch, a slow
// disk) does not throw away the whole buffer; fresh_sample_count trims old
// audio at save time anyway.
constexpr uint64_t kAudioGapNs = 2'000'000'000;

// Whether audio arriving at now_ns follows a pause long enough that the
// buffered audio should be dropped first. last_push_ns of 0 means nothing
// has arrived yet.
bool audio_resumed_after_gap(uint64_t last_push_ns, uint64_t now_ns);

// How many of the newest `buffered` samples were heard within the last
// `window_samples` samples' worth of time before now_ns, given the newest
// sample arrived at last_push_ns. The rest are older than the window.
std::size_t fresh_sample_count(std::size_t buffered, std::size_t window_samples, uint32_t sample_rate,
			       uint64_t last_push_ns, uint64_t now_ns);
