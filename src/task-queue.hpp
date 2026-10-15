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

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

// Runs tasks one at a time, in order, on its own thread. The destructor runs
// whatever is still queued and then joins the thread, so no task outlives its
// owner (or the plugin, once OBS unloads it). Kept free of OBS so the tests
// can run it on its own.
class TaskQueue {
public:
	// At most max_pending tasks wait at once; push() refuses more, so a
	// held-down hotkey cannot pile up gigabytes of audio.
	explicit TaskQueue(std::size_t max_pending);
	~TaskQueue();

	TaskQueue(const TaskQueue &) = delete;
	TaskQueue &operator=(const TaskQueue &) = delete;

	// Queues a task. Returns false, and drops it, when max_pending tasks are
	// already waiting.
	bool push(std::function<void()> task);

private:
	void run();

	const std::size_t max_pending;
	std::mutex lock;
	std::condition_variable wake;
	std::deque<std::function<void()>> tasks;
	bool stopping = false;
	std::thread worker;
};
