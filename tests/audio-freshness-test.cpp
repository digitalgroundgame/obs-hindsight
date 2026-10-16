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
#include "check.hpp"

#include <cstdint>

namespace {

constexpr uint64_t kSecond = 1'000'000'000;
constexpr uint32_t kRate = 48000;
constexpr std::size_t kWindow = 30 * kRate;

void nothing_heard_yet()
{
	CHECK(fresh_sample_count(0, kWindow, kRate, 0, 100 * kSecond) == 0);
	CHECK(fresh_sample_count(kWindow, kWindow, kRate, 0, 100 * kSecond) == 0);
	CHECK(!audio_resumed_after_gap(0, 100 * kSecond));
}

void live_audio_is_all_fresh()
{
	const uint64_t last = 1000 * kSecond;
	CHECK(fresh_sample_count(kWindow, kWindow, kRate, last, last) == kWindow);
	CHECK(fresh_sample_count(1000, kWindow, kRate, last, last + kSecond / 100) == 1000);
}

void quiet_time_ages_out_the_oldest_audio()
{
	const uint64_t last = 1000 * kSecond;
	// 10 s since the newest sample: only the newest 20 s are inside the window.
	CHECK(fresh_sample_count(kWindow, kWindow, kRate, last, last + 10 * kSecond) == 20 * kRate);
	// Half a second.
	CHECK(fresh_sample_count(kWindow, kWindow, kRate, last, last + kSecond / 2) == kWindow - kRate / 2);
}

void audio_older_than_the_window_is_dropped()
{
	const uint64_t last = 1000 * kSecond;
	CHECK(fresh_sample_count(kWindow, kWindow, kRate, last, last + 30 * kSecond) == 0);
	CHECK(fresh_sample_count(kWindow, kWindow, kRate, last, last + 3 * 3600 * kSecond) == 0);
	// Days of silence must not overflow into a large count.
	CHECK(fresh_sample_count(kWindow, kWindow, kRate, last, last + 40 * 24 * 3600 * kSecond) == 0);
}

void clock_going_backwards_counts_as_no_time()
{
	const uint64_t last = 1000 * kSecond;
	CHECK(fresh_sample_count(500, kWindow, kRate, last, last - kSecond) == 500);
	CHECK(!audio_resumed_after_gap(last, last - kSecond));
}

void gap_detection()
{
	const uint64_t last = 1000 * kSecond;
	CHECK(!audio_resumed_after_gap(last, last + 21'000'000));
	CHECK(!audio_resumed_after_gap(last, last + kAudioGapNs));
	CHECK(audio_resumed_after_gap(last, last + kAudioGapNs + 1));
	CHECK(audio_resumed_after_gap(last, last + 3600 * kSecond));
}

} // namespace

int main()
{
	nothing_heard_yet();
	live_audio_is_all_fresh();
	quiet_time_ages_out_the_oldest_audio();
	audio_older_than_the_window_is_dropped();
	clock_going_backwards_counts_as_no_time();
	gap_detection();
	return check::finish();
}
