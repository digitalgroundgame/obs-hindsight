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
#include <utility>

AudioRing::AudioRing()
{
	deque_init(&queue);
}

AudioRing::~AudioRing()
{
	deque_free(&queue);
}

AudioRing::AudioRing(AudioRing &&other) noexcept : queue(other.queue), max_samples(other.max_samples)
{
	deque_init(&other.queue);
	other.max_samples = 0;
}

AudioRing &AudioRing::operator=(AudioRing &&other) noexcept
{
	std::swap(queue, other.queue);
	std::swap(max_samples, other.max_samples);
	return *this;
}

void AudioRing::reset(std::size_t capacity)
{
	deque_free(&queue);
	deque_reserve(&queue, capacity * sizeof(float));
	max_samples = capacity;
}

void AudioRing::clear()
{
	deque_pop_front(&queue, nullptr, queue.size);
}

void AudioRing::push_planar(const float *const *planes, std::size_t channels, std::size_t frames)
{
	if (max_samples == 0 || channels == 0)
		return;

	for (std::size_t frame = 0; frame < frames; frame++) {
		float sum = 0.0f;
		for (std::size_t ch = 0; ch < channels; ch++)
			sum += planes[ch] ? planes[ch][frame] : 0.0f;
		const float sample = sum / static_cast<float>(channels);

		// Drop the oldest sample first, so the queue never outgrows the
		// space reset() reserved and never reallocates here.
		if (size() == max_samples)
			deque_pop_front(&queue, nullptr, sizeof(float));
		deque_push_back(&queue, &sample, sizeof(float));
	}
}

std::vector<float> AudioRing::snapshot() const
{
	std::vector<float> out;
	out.reserve(size());
	copy_to(out);
	return out;
}

void AudioRing::copy_to(std::vector<float> &out, std::size_t newest) const
{
	const std::size_t count = std::min(size(), newest);
	out.resize(count);
	if (count > 0)
		// deque_peek_back only reads, but libobs declares it non-const.
		deque_peek_back(const_cast<deque *>(&queue), out.data(), count * sizeof(float));
}

void AudioRing::take_newest_from(const AudioRing &other)
{
	if (&other == this)
		return;

	clear();
	const std::size_t bytes = std::min(other.size(), max_samples) * sizeof(float);
	if (bytes == 0)
		return;

	// clear() left the queue starting at 0, so growing it within the
	// reserved space gives one run of bytes to copy straight into.
	deque_upsize(&queue, bytes);
	// deque_peek_back only reads, but libobs declares it non-const.
	deque_peek_back(const_cast<deque *>(&other.queue), deque_data(&queue, 0), bytes);
}
